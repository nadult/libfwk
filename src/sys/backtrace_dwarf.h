// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// This file contains snippets of code from backward:
// https://github.com/bombela/backward-cpp.git
// License is available in extern/backward-license.txt

#pragma once

#include "fwk/dynamic.h"
#include "fwk/sys/backtrace.h"
#include "fwk/vector.h"

#ifdef FWK_PLATFORM_MINGW
#include <dwarf.h>
#include <libdwarf.h>
#else
#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>
#include <libelf.h>
#endif

#include <unistd.h>

namespace fwk {

struct ResolvedTrace {
	struct SourceLoc {
		string function;
		string filename;
		unsigned line;
		unsigned col;

		SourceLoc() : line(0), col(0) {}

		bool operator==(const SourceLoc &b) const {
			return function == b.function && filename == b.filename && line == b.line &&
				   col == b.col;
		}

		bool operator!=(const SourceLoc &b) const { return !(*this == b); }
	};

	void *addr = nullptr;

	// In which binary object this trace is located.
	string object_filename;

	// The function in the object that contain the trace. This is not the same
	// as source.function which can be an function inlined in object_function.
	string object_function;

	// The source location of this trace. It is possible for filename to be
	// empty and for line/col to be invalid (value 0) if this information
	// couldn't be deduced, for example if there is no debug information in the
	// binary object.
	SourceLoc source;

	// An optionals list of "inliners". All the successive sources location
	// from where the source location of the trace (the attribute right above)
	// is inlined. It is especially useful when you compiled with optimization.
	vector<SourceLoc> inliners;
};

// TODO: it would be better to use LLVM's Debug info instead...
class DwarfResolver {
  public:
	void resolve(ResolvedTrace &);

  private:
	struct DieCacheEntry {
		vector<Pair<Dwarf_Off>> spec_section;
		vector<Pair<Dwarf_Addr, int>> line_section;
		Dwarf_Line *line_buffer = nullptr;
		Dwarf_Signed line_count = 0;
		Dwarf_Line_Context line_context = 0;

		bool isEmpty() {
			return line_buffer == NULL || line_count == 0 || line_context == NULL ||
				   line_section.empty();
		}

		~DieCacheEntry() {
			if(line_context)
				dwarf_srclines_dealloc_b(line_context);
		}
	};

	struct FileObject {
		FileObject() = default;
		~FileObject();
		FileObject(const FileObject &) = delete;
		void operator=(const FileObject &) = delete;

		int file_handle = 0;
#ifdef FWK_PLATFORM_LINUX
		Elf *elf_handle = nullptr;
#endif
		Dwarf_Debug dwarf_handle = 0;

		vector<Pair<uintptr_t, string>> symbol_cache;
		vector<Dynamic<DieCacheEntry>> die_cache;
		vector<Dwarf_Off> die_offsets;
		DieCacheEntry *current_cu;
	};

	template <int bits> bool getElfData(FileObject &, string &debuglink);
	FileObject &load_object_with_dwarf(const string &filename_object);

	DieCacheEntry &get_die_cache(FileObject &fobj, Dwarf_Die die);

	static Dwarf_Die get_referenced_die(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Half attr,
										bool global);

	static string get_referenced_die_name(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Half attr,
										  bool global);

	// Returns a spec DIE linked to the passed one. The caller should deallocate the DIE
	static Dwarf_Die get_spec_die(FileObject &fobj, Dwarf_Die die);

	static bool die_has_pc(FileObject &fobj, Dwarf_Die die, Dwarf_Addr pc);

	static void get_type(Dwarf_Debug dwarf, Dwarf_Die die, string &type);

	static string get_type_by_signature(Dwarf_Debug dwarf, Dwarf_Die die);

	struct type_context_t {
		bool is_const = false;
		bool is_typedef = false;
		bool has_type = false;
		bool has_name = false;
		string text;
	};

	// Types are resolved from right to left: we get the variable name first
	// and then all specifiers (like const or pointer) in a chain of DW_AT_type
	// DIEs. Call this function recursively until we get a complete type
	// string.
	static void set_parameter_string(FileObject &fobj, Dwarf_Die die, type_context_t &context);

	// Resolve the function return type and parameters
	static void set_function_parameters(string &name, vector<string> &ns, FileObject &fobj,
										Dwarf_Die die);

	struct InlinersSearchCB {
		void operator()(Dwarf_Die die, vector<string> &ns);
		ResolvedTrace &trace;
		FileObject &fobj;
		Dwarf_Die cu_die;
	};

	static Dwarf_Die find_fundie_by_pc(FileObject &fobj, Dwarf_Die parent_die, Dwarf_Addr pc,
									   Dwarf_Die result);

	template <typename CB>
	static bool deep_first_search_by_pc(FileObject &fobj, Dwarf_Die parent_die, Dwarf_Addr pc,
										vector<string> &ns, CB cb);

	static string die_call_file(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Die cu_die);
	Dwarf_Die find_die(FileObject &fobj, Dwarf_Addr addr);

	bool dwarf_loaded = false;

	vector<string> file_names;
	vector<Dynamic<FileObject>> file_objects;
};

}
