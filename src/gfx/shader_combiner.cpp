// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/shader_combiner.h"
#include "fwk/io/file_system.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

using PieceSet = vector<ShaderPieceId>;

DEFINE_ENUM(ShaderCommandId, include);

struct ShaderCommand {
	ShaderCommandId id;
	vector<string> params;
};

static void ensureEndLine(string &text) {
	if(!text.ends_with('\n'))
		text += '\n';
}

static int countLines(Str text) {
	return countIf(text, [](char c) { return c == '\n'; });
}

static Ex<vector<ShaderCommand>> parseShaderCommands(const string &text) {
	size_t pos = 0;

	vector<ShaderCommand> out;
	if(text.find("$$") == string::npos)
		return out;

	auto lines = tokenize(text, '\n');
	for(int n : intRange(lines)) {
		ON_FAIL("While parsing line: %", n + 1);

		auto &line = lines[n];
		if(line.find("$$") == string::npos)
			continue;
		if(line.find("// $$") != 0)
			return ERROR("Line with shader command should start with: // $$command_name");
		if(!isalnum(line[5]))
			return ERROR("Shader command name should immediately follow $$");

		TextParser parser(line);
		parser.advance(5);

		vector<string> commands;
		while(!parser.empty()) {
			Str value;
			parser >> value;
			commands.emplace_back(value);
		}

		if(!commands)
			return ERROR("No shader command given");

		if(auto cmd = maybeFromString<ShaderCommandId>(commands[0])) {
			commands.erase(commands.begin());
			out.emplace_back(*cmd, move(commands));
		} else
			return ERROR("Invalid command: %", commands[0]);
	}

	return out;
}

Ex<PProgram> CombinedShaderSource::compileAndLink(vector<string> locations) const {
	PShader shaders[count<ShaderType>];

	if(isCompute())
		ASSERT(locations.empty() && "It makes no sense to specify locations for compute shader");

	int count = 0;
	for(auto type : all<ShaderType>)
		if(!sources[type].empty()) {
			auto shader = GlShader::compile(type, sources[type]);
			if(!shader->isCompiled()) {
				auto log = shader->compilationLog();
				log = translateLog(log);
				return ERROR("Error while compiling % shader in program '%':\n%", type, name, log);
			}
			shaders[count++] = move(shader);
		}

	auto program = GlProgram::link({shaders, count}, locations);
	if(!program->isLinked()) {
		auto log = program->linkLog();
		log = translateLog(log);
		return ERROR("Error while linking program '%':\n%", name, log);
	}
	return program;
}

bool CombinedShaderSource::isCompute() const { return !sources[ShaderType::compute].empty(); }

// Errors from different vendors:
// nvidia: source_id(line_number) : error/warning CXXXX: ...
// intel:  source_id:line_number(character_number): error/warning: ...
// amd:    ERROR: 0:63: error(#132) Syntax error: "-" parse error
//         ERROR: error(#273) 1 compilation errors.  No code generated
string CombinedShaderSource::translateLog(ZStr text) const {
	auto lines = tokenize(text, '\n');
	auto vendor = gl_info->vendor;

	for(auto &line : lines) {
		int source_id = -1, line_nr = -1, char_pos = -1;
		Str text;

		if(vendor == GlVendor::nvidia) {
			auto pos1 = line.find('(');
			auto pos2 = line.find(") : ");

			if(isOneOf(string::npos, pos1, pos2) || pos1 >= pos2)
				continue;

			line[pos1] = 0;
			line[pos2] = 0;

			source_id = tryFromString<uint>(line.data(), -1);
			line_nr = tryFromString<uint>(line.data() + pos1 + 1, -1);
			text = line.data() + pos2 + 4;
		} else if(vendor == GlVendor::intel) {
			auto pos1 = line.find(':');
			auto pos2 = line.find("(");
			auto pos3 = line.find("): ");

			if(isOneOf(string::npos, pos1, pos2, pos3) || !(pos1 < pos2 && pos2 < pos3))
				continue;

			line[pos1] = 0;
			line[pos2] = 0;
			line[pos3] = 0;

			source_id = tryFromString<uint>(line.data(), -1);
			line_nr = tryFromString<uint>(line.data() + pos1 + 1, -1);
			char_pos = tryFromString<uint>(line.data() + pos2 + 1, -1);

			if(char_pos == -1)
				continue;
			text = line.data() + pos3 + 3;
		} else {
			// TODO: amd: write me
		}

		if(line_nr > 0) {
			int label_id = lineToLabel(labels, line_nr);
			if(label_id != -1) {
				auto &label = labels[label_id];
				line_nr = line_nr - label.second + 1;
				if(char_pos >= 0)
					line = format("%:%(%): %", label.first, line_nr, char_pos, text);
				else
					line = format("%:%: %", label.first, line_nr, text);
			}
		}
	}

	// Merges lines and adds small indent
	TextFormatter fmt;
	for(int n = 0; n < lines.size(); n++)
		fmt(n + 1 != lines.size() ? "  %\n" : "  %", lines[n]);
	return fmt.text();
}

int CombinedShaderSource::lineToLabel(CSpan<Pair<string, int>> labels, int line) {
	int best = -1;
	for(int i = 0; i < labels.size(); i++) {
		int label_line = labels[i].second;

		if(label_line <= line && (best == -1 || label_line > labels[best].second))
			best = i;
	}
	return best;
}

ShaderCombiner::ShaderCombiner(CSpan<string> names, CSpan<FilePath> paths) {
	ASSERT(names.size() == paths.size());

	m_pieces.reserve(names.size());
	for(int i = 0; i < names.size(); i++) {
		Piece new_piece;
		new_piece.name = names[i];
		new_piece.path = paths[i];
		m_name_map.emplace(names[i], PieceId(m_pieces.size()));
		m_pieces.emplace_back(move(new_piece));
	}
}

FWK_COPYABLE_CLASS_IMPL(ShaderCombiner);

void topoSort(Span<ShaderCombiner::Piece> pieces, int &counter, int cur) {
	auto &piece = pieces[cur];
	if(piece.topological_index != -1)
		return;
	piece.topological_index = 0;
	for(auto dep : piece.deps)
		topoSort(pieces, counter, dep);
	piece.topological_index = counter++;
}

Ex<void> ShaderCombiner::loadPieces() {
	for(auto &piece : m_pieces) {
		auto source = EX_PASS(loadShader(piece.path));
		piece.code = move(source.code);
		ensureEndLine(piece.code);
		piece.num_lines = countLines(piece.code);
		piece.deps = move(source.pieces);
	}

	// Passing dependencies through
	bool do_update = true;
	while(do_update) {
		do_update = false;
		for(auto &piece : m_pieces) {
			int old_size = piece.deps.size();
			piece.deps = completeSet(piece.deps);
			do_update |= piece.deps.size() != old_size;
		}
	}

	int counter = 0;
	for(auto &piece : m_pieces)
		piece.topological_index = -1;
	for(int i : intRange(m_pieces))
		topoSort(m_pieces, counter, i);

	for(int i = 0; i < m_pieces.size(); i++)
		for(auto dep : m_pieces[i].deps)
			if(dep == i)
				return ERROR("Circular dependency: '%' depends on itself", m_pieces[i].name);
	return {};
}

vector<Str> ShaderCombiner::pieceNames(const PieceSet &set) const {
	return transform(set, [&](PieceId id) -> Str { return m_pieces[id].name; });
}

Ex<ShaderSource> ShaderCombiner::loadShader(ZStr path) const {
	ShaderSource out;
	out.name = path;
	out.path = path;
	out.code = EX_PASS(loadFileString(path));
	ON_FAIL("While loading shader code from '%'", path);
	out.pieces = EX_PASS(parseDependencies(out.code));
	return out;
}

Ex<PieceSet> ShaderCombiner::parseDependencies(Str code) const {
	auto commands = EX_PASS(parseShaderCommands(code));
	PieceSet pieces;

	for(auto &command : commands) {
		if(command.id == ShaderCommandId::include) {
			for(auto &param : command.params) {
				auto id = find(param);
				if(!id)
					return ERROR("Cannot find shader piece: '%'", param);
				pieces.emplace_back(*id);
			}
		}
	}

	makeSortedUnique(pieces);
	return completeSet(pieces);
}

static const EnumMap<ShaderType, Str> shader_macros = {{{ShaderType::vertex, "VERTEX_SHADER"},
														{ShaderType::fragment, "FRAGMENT_SHADER"},
														{ShaderType::geometry, "GEOMETRY_SHADER"},
														{ShaderType::compute, ""}}};

static string separator(Str name, int max_len = 80) {
	TextFormatter fmt;
	int len = max(3, (max_len - name.size() - 5) / 2);
	fmt << "\n// ";
	for(int i : intRange(len))
		fmt << '=';
	fmt << ' ' << name << ' ';
	for(int i : intRange(len))
		fmt << '=';
	fmt << '\n';
	return fmt.text();
}

CombinedShaderSource ShaderCombiner::combine(const ShaderSource &source,
											 EnumFlags<ShaderType> types) const {
	CombinedShaderSource out;

	if(types & ShaderType::compute) {
		ASSERT(types == ShaderType::compute &&
			   "It's illegal to combine compute shader with other shader types");
	}

	string defs_source = source.defs;
	ensureEndLine(defs_source);
	int line_offset = countLines(defs_source) + 1;
	if(types != ShaderType::compute)
		line_offset++;

	string main_source;
	auto pieces = completeSet(source.pieces);
	out.labels.resize(pieces.size() + 2);
	out.labels[0] = {"DEFINITIONS", 1};
	for(int i : intRange(pieces)) {
		auto &piece = m_pieces[pieces[i]];
		main_source += separator(piece.name);
		line_offset += 2;
		out.labels[i + 1] = {piece.name, line_offset};
		main_source += piece.code;
		line_offset += piece.num_lines;
	}

	main_source += separator(source.name);
	main_source += source.code;
	ensureEndLine(main_source);
	out.labels.back() = {source.name, line_offset + 2};

	if(!types) {
		types = ShaderType::vertex | ShaderType::fragment;
		auto macro = shader_macros[ShaderType::geometry];
		if(main_source.find(macro) != string::npos || defs_source.find(macro) != string::npos)
			types |= ShaderType::geometry;
	}

	for(auto type : types) {
		string macro = shader_macros[type];
		if(!macro.empty())
			macro = "#define " + macro + "\n";
		out.sources[type] = defs_source + macro + main_source;
	}

	out.name = source.name;
	return out;
}

Maybe<ShaderPieceId> ShaderCombiner::find(Str name) const {
	auto it = m_name_map.find(name);
	return it == m_name_map.end() ? Maybe<PieceId>() : it->value;
}

ShaderPieceId ShaderCombiner::get(Str name) const {
	auto result = find(name);
	if(!result) {
		FATAL("Couldn't find shader piece: '%s'\nAvailable pieces: %s", string(name).c_str(),
			  toString(m_name_map.keys()).c_str());
	}
	return *result;
}

ShaderPieceSet ShaderCombiner::completeSet(const PieceSet &set) const {
	vector<bool> bits(m_pieces.size(), false);
	for(auto dep1 : set) {
		bits[dep1] = true;
		for(auto dep2 : m_pieces[dep1].deps)
			bits[dep2] = true;
	}

	PieceSet out;
	for(int i = 0; i < m_pieces.size(); i++)
		if(bits[i])
			out.emplace_back(PieceId(i));
	std::sort(out.begin(), out.end(), [&](PieceId a, PieceId b) {
		return m_pieces[a].topological_index < m_pieces[b].topological_index;
	});
	return out;
}

}
