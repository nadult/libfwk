// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/platform.h"

#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#include <dwrite.h>
#endif

#include "fwk/gfx/font_finder.h"
#include "fwk/io/file_system.h"
#include "fwk/math/constants.h"
#include "fwk/parse.h"
#include "fwk/pod_vector.h"
#include "fwk/sys/expected.h"

namespace fwk {
#ifdef FWK_PLATFORM_WINDOWS
string wideToUtf8(CSpan<wchar_t> text) {
	// TODO: make sure that it's properly 0-terminated
	PodVector<char> buffer(text.size() * 3);
	auto size = WideCharToMultiByte(CP_UTF8, 0, text.data(), text.size(), buffer.data(),
									buffer.size(), 0, 0);
	return string(buffer.data());
}
#endif

vector<SystemFont> listSystemFonts() {
	vector<SystemFont> out;
#ifdef FWK_PLATFORM_WINDOWS
	IDWriteFactory *factory = nullptr;
	IDWriteFontCollection *collection = nullptr;

	auto result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
									  reinterpret_cast<IUnknown **>(&factory));
	if(SUCCEEDED(result))
		result = factory->GetSystemFontCollection(&collection);
	int family_count = SUCCEEDED(result) ? collection->GetFontFamilyCount() : 0;

	for(uint i = 0; i < family_count; i++) {
		IDWriteFontFamily *family = nullptr;
		IDWriteLocalizedStrings *family_names = nullptr;
		IDWriteFontList *matching_fonts = nullptr;
		wchar_t wfamily_name[2048];
		string family_name;
		uint family_name_index = 0;
		BOOL exists = false;

		result = collection->GetFontFamily(i, &family);
		if(SUCCEEDED(result))
			result = family->GetFamilyNames(&family_names);
		if(SUCCEEDED(result))
			family->GetMatchingFonts(DWRITE_FONT_WEIGHT_THIN, DWRITE_FONT_STRETCH_UNDEFINED,
									 DWRITE_FONT_STYLE_NORMAL, &matching_fonts);
		uint num_fonts = SUCCEEDED(result) ? matching_fonts->GetFontCount() : 0;
		if(SUCCEEDED(result))
			result = family_names->FindLocaleName(L"en-us", &family_name_index, &exists);
		if(SUCCEEDED(result))
			result =
				family_names->GetString(family_name_index, wfamily_name, arraySize(wfamily_name));
		if(SUCCEEDED(result))
			family_name = wideToUtf8(wfamily_name);

		for(uint j = 0; j < num_fonts; j++) {
			IDWriteFont *font = nullptr;
			IDWriteFontFace *face = nullptr;
			IDWriteLocalizedStrings *names = nullptr;
			vector<IDWriteFontFile *> files;
			uint num_files = 0, name_index = 0;
			BOOL exists = 0;
			wchar_t wname[2048];
			auto result = matching_fonts->GetFont(j, &font);

			auto ms_style = font->GetStyle();
			SystemFontStyle style = ms_style == DWRITE_FONT_STYLE_NORMAL ? SystemFontStyle::normal
									: ms_style == DWRITE_FONT_STYLE_OBLIQUE
										? SystemFontStyle::oblique
										: SystemFontStyle::italic;

			auto weight = (int)font->GetWeight();
			auto stretch = (int)font->GetStretch();

			if(SUCCEEDED(result))
				result = font->GetFaceNames(&names);
			if(SUCCEEDED(result))
				result = font->CreateFontFace(&face);
			if(SUCCEEDED(result))
				result = face->GetFiles(&num_files, nullptr);
			if(SUCCEEDED(result)) {
				files.resize(num_files);
				result = face->GetFiles(&num_files, files.data());
			}
			if(!SUCCEEDED(result))
				files.clear();
			if(SUCCEEDED(result))
				result = names->FindLocaleName(L"en-us", &name_index, &exists);
			if(SUCCEEDED(result))
				result = names->GetString(name_index, wname, arraySize(wname));

			for(auto &file : files) {
				if(!file)
					continue;
				IDWriteFontFileLoader *loader = nullptr;
				IDWriteLocalFontFileLoader *local_loader = nullptr;

				const void *ref_key = nullptr;
				uint ref_key_size = 0;
				wchar_t wfile_path[2048];

				auto result = file->GetReferenceKey(&ref_key, &ref_key_size);
				if(SUCCEEDED(result))
					result = file->GetLoader(&loader);
				if(SUCCEEDED(result))
					result = loader->QueryInterface(&local_loader);
				if(SUCCEEDED(result)) {
					result = local_loader->GetFilePathFromKey(ref_key, ref_key_size, wfile_path,
															  arraySize(wfile_path));
				}
				if(SUCCEEDED(result)) {
					out.emplace_back(family_name, wideToUtf8(wname), wideToUtf8(wfile_path),
									 SystemFontParams{weight, stretch, style});
				}
				if(loader)
					loader->Release();
				if(file)
					file->Release();
			}

			if(face)
				face->Release();
			if(names)
				names->Release();
			if(font)
				font->Release();
		}

		if(matching_fonts)
			matching_fonts->Release();
		if(family_names)
			family_names->Release();
		if(family)
			family->Release();
	}

	if(collection)
		collection->Release();
	if(factory)
		factory->Release();
#elif defined(FWK_PLATFORM_LINUX)
	auto result = execCommand("fc-list : file family style slant weight width");
	if(!result)
		return out;

	auto parse_param = [](Str params, Str param_name) -> Str {
		int pos = params.find(param_name);
		if(pos == -1)
			return "";
		params = params.substr(pos + param_name.size());
		pos = params.find(':');
		if(pos != -1)
			params = params.substr(0, pos);
		pos = params.find(',');
		if(pos != -1)
			params = params.substr(0, pos);
		return params;
	};

	for(auto line : tokenize(result->first, '\n')) {
		auto delim = line.find(':');
		if(delim == string::npos)
			continue;
		string file_path = line.substr(0, delim);

		delim++;
		while(isspace(line[delim]))
			delim++;

		string name;
		while(!isOneOf(line[delim], ':', ',', 0)) {
			char c = line[delim];
			if(c == '\\' && line[delim + 1])
				c = line[++delim];
			name += c;
			delim++;
		}

		if(delim == ',')
			delim = line.find(':', delim);
		Str params(line.data() + delim, line.size() - delim);

		auto style_str = parse_param(params, "style=");
		auto slant_str = parse_param(params, "slant=");
		auto weight_str = parse_param(params, "weight=");
		auto width_str = parse_param(params, "width=");

		if(!style_str || !slant_str || !weight_str || !width_str)
			continue;

		int islant = tryFromString<int>(string(slant_str), 0);
		int weight = tryFromString<int>(string(slant_str), 80);
		int width = tryFromString<int>(string(slant_str), 100);

		SystemFontParams font_params;
		font_params.style = islant == 0		? SystemFontStyle::normal
							: islant == 100 ? SystemFontStyle::italic
											: SystemFontStyle::oblique;

		// 0 - 210(210) -> 100 - 950(850)
		font_params.weight = int(double(weight) * 850 / 210) + 100;
		// 50 - 200(150)     -> 1 - 10(9)
		font_params.stretch = int(double(width - 50) * 9 / 150) + 1;
		out.emplace_back(name, style_str, file_path, font_params);
	}
#else
#endif
	return out;
}

Maybe<int> findBestFont(CSpan<SystemFont> fonts, CSpan<Str> family_names, SystemFontParams params) {
	Maybe<int> out;
	double best_dist = inf;

	// TODO: optimize?
	for(int i = 0; i < fonts.size(); i++) {
		auto &font = fonts[i];

		int name_index = -1;
		for(int j = 0; j < family_names.size(); j++) {
			if(family_names[j].compareIgnoreCase(font.family) == 0) {
				name_index = j;
				break;
			}
		}
		if(name_index == -1)
			continue;

		double distance = name_index * 10.0 + abs(int(params.style) - int(font.params.style)) +
						  abs(params.weight - font.params.weight) * 0.001 +
						  abs(params.stretch - font.params.stretch) * 0.00001;
		if(distance < best_dist) {
			best_dist = distance;
			out = i;
		}
	}

	return out;
}
}
