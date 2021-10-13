// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx_base.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/io/file_system.h"
#include "fwk/tag_id.h"
#include "fwk/vector.h"

namespace fwk {

using ShaderPieceId = TagId<Tag::shader_piece>;
using ShaderPieceSet = vector<ShaderPieceId>;

// Shader program loaded from single .shader file
// It can include multiple shader pieces
struct ShaderSource {
	string name, path, code, defs;
	ShaderPieceSet pieces;
};

struct CombinedShaderSource {
	Ex<PProgram> compileAndLink(vector<string> locations = {}) const;

	// Translates source locations in shader compilation/linking log.
	// Maps lines from merged shader source into different shader pieces.
	string translateLog(ZStr log) const;

	// Returns -1 if no label matches
	static int lineToLabel(CSpan<Pair<string, int>> labels, int line_id);

	bool isCompute() const;

	string name;

	// Sources for different shader types are identical except for
	// the line with shader type macro definition.
	EnumMap<ShaderType, string> sources;

	// Maps pieces to line ranges
	// Each pair contains: piece name & first line offset (starting from 1)
	vector<pair<string, int>> labels;
};

// Manages shaders combined from different pieces.
// All shader pieces are loaded from specified directory.
// Pieces can depend on each other. To specify that some piece of
// code depends on another, $$include command can be used like so.
// Pieces names & paths are immutable, but they can be reloaded multiple times.
class ShaderCombiner {
  public:
	using PieceId = ShaderPieceId;
	using PieceSet = ShaderPieceSet;

	struct Piece {
		string name;
		string code;
		FilePath path;
		vector<PieceId> deps;
		int topological_index = 0;
		int num_lines = 0;
	};

	ShaderCombiner(CSpan<string> names, CSpan<FilePath> paths);
	FWK_COPYABLE_CLASS(ShaderCombiner);

	// Actually loads pieces source code
	Ex<void> loadPieces();

	Ex<PieceSet> parseDependencies(Str code) const;
	Ex<ShaderSource> loadShader(ZStr path) const;
	vector<Str> pieceNames(const PieceSet &) const;

	// Combines shader code, definitions & pieces into single chunk, ready for compilation.
	// You can specify which shader types you want. By default fragment & vertex shaders
	// are enabled. Geometry shader as well if GEOMETRY_SHADER is defined in source.
	// For fragment, vertex & geometry shaders appropriate macro definition will be added
	// at the end of definitions block (VERTEX_SHADER, FRAGMENT_SHADER or GEOMETRY_SHADER).
	CombinedShaderSource combine(const ShaderSource &, EnumFlags<ShaderType> = none) const;

	Maybe<PieceId> find(Str name) const;
	PieceId get(Str name) const;

	SimpleIndexRange<PieceId> pieceIds() const { return {0, m_pieces.size()}; }
	const Piece &operator[](PieceId id) const { return m_pieces[id]; }

  private:
	PieceSet completeSet(const PieceSet &) const;

	HashMap<string, PieceId> m_name_map;
	vector<Piece> m_pieces;
};
}
