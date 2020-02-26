// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// This file contains snippets of code from backward:
// https://github.com/bombela/backward-cpp.git
// License is available in extern/backward-license.txt

#pragma once

#include "fwk/dynamic.h"
#include "fwk/sys/backtrace.h"
#include "fwk/vector.h"

#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>
#include <libelf.h>

#include <map>
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
	typedef vector<SourceLoc> source_locs_t;
	source_locs_t inliners;
};

class DwarfResolver {
  public:
	void resolve(ResolvedTrace &);

  private:
	typedef std::map<Dwarf_Addr, int> die_linemap_t;
	typedef std::map<Dwarf_Off, Dwarf_Off> die_specmap_t;

	struct die_cache_entry {
		die_specmap_t spec_section;
		die_linemap_t line_section;
		Dwarf_Line *line_buffer = nullptr;
		Dwarf_Signed line_count = 0;
		Dwarf_Line_Context line_context = 0;

		bool isEmpty() {
			return line_buffer == NULL || line_count == 0 || line_context == NULL ||
				   line_section.empty();
		}

		~die_cache_entry() {
			if(line_context)
				dwarf_srclines_dealloc_b(line_context);
		}
	};

	typedef std::map<Dwarf_Off, die_cache_entry> die_cache_t;
	typedef std::map<uintptr_t, string> symbol_cache_t;

	struct dwarf_fileobject {
		dwarf_fileobject() = default;
		~dwarf_fileobject() {
			if(file_handle)
				::close(file_handle);
			if(elf_handle)
				elf_end(elf_handle);
			if(dwarf_handle)
				dwarf_finish(dwarf_handle, nullptr);
		}
		dwarf_fileobject(const dwarf_fileobject &) = delete;
		void operator=(const dwarf_fileobject &) = delete;

		int file_handle = 0;
		Elf *elf_handle = nullptr;
		Dwarf_Debug dwarf_handle = 0;
		symbol_cache_t symbol_cache;

		// Die cache
		die_cache_t die_cache;
		die_cache_entry *current_cu;
	};

	dwarf_fileobject &load_object_with_dwarf(const string &filename_object);

	die_cache_entry &get_die_cache(dwarf_fileobject &fobj, Dwarf_Die die);

	static Dwarf_Die get_referenced_die(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Half attr,
										bool global);

	static string get_referenced_die_name(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Half attr,
										  bool global);

	// Returns a spec DIE linked to the passed one. The caller should
	// deallocate the DIE
	static Dwarf_Die get_spec_die(dwarf_fileobject &fobj, Dwarf_Die die);

	static bool die_has_pc(dwarf_fileobject &fobj, Dwarf_Die die, Dwarf_Addr pc);

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
	static void set_parameter_string(dwarf_fileobject &fobj, Dwarf_Die die,
									 type_context_t &context);

	// Resolve the function return type and parameters
	static void set_function_parameters(string &function_name, vector<string> &ns,
										dwarf_fileobject &fobj, Dwarf_Die die);

	// defined here because in C++98, template function cannot take locally
	// defined types... grrr.
	struct inliners_search_cb {
		void operator()(Dwarf_Die die, vector<string> &ns);

		ResolvedTrace &trace;
		dwarf_fileobject &fobj;
		Dwarf_Die cu_die;
		inliners_search_cb(ResolvedTrace &t, dwarf_fileobject &f, Dwarf_Die c)
			: trace(t), fobj(f), cu_die(c) {}
	};

	static Dwarf_Die find_fundie_by_pc(dwarf_fileobject &fobj, Dwarf_Die parent_die, Dwarf_Addr pc,
									   Dwarf_Die result);

	template <typename CB>
	static bool deep_first_search_by_pc(dwarf_fileobject &fobj, Dwarf_Die parent_die, Dwarf_Addr pc,
										vector<string> &ns, CB cb);

	static string die_call_file(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Die cu_die);
	Dwarf_Die find_die(dwarf_fileobject &fobj, Dwarf_Addr addr);

	bool _dwarf_loaded = false;

	vector<string> file_names;
	vector<Dynamic<dwarf_fileobject>> file_objects;
};

}
