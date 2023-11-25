// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// This file contains snippets of code from backward:
// https://github.com/bombela/backward-cpp.git
// License is available in extern/backward-license.txt

#include "backtrace_dwarf.h"

#include "fwk/algorithm.h"
#include "fwk/io/file_system.h"

#ifdef FWK_DWARF_DISABLED
#error "This file should only be compiled when FWK_DWARF is enabled"
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__GLIBC__) && defined(FWK_PLATFORM_LINUX)
#define WITH_LINK_MAP
#include <link.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <dlfcn.h>
#undef _GNU_SOURCE
#else
#include <dlfcn.h>
#endif

#endif

#include <fstream>
#include <iostream>

namespace fwk {

#ifdef FWK_PLATFORM_LINUX

static string read_symlink(string const &symlink_path) {
	string path;
	path.resize(100);

	while(true) {
		ssize_t len = ::readlink(symlink_path.c_str(), &*path.begin(), path.size());
		if(len < 0) {
			return "";
		}
		if(static_cast<size_t>(len) == path.size()) {
			path.resize(path.size() * 2);
		} else {
			path.resize(static_cast<string::size_type>(len));
			break;
		}
	}

	return path;
}

// TODO: simplify it
static string resolve_exec_path(Dl_info &symbol_info) {
	string argv0;
	std::getline(std::ifstream("/proc/self/cmdline"), argv0, '\0');

	// mutates symbol_info.dli_fname to be filename to open and returns filename to display
	if(symbol_info.dli_fname == argv0) {
		// dladdr returns argv[0] in dli_fname for symbols contained in
		// the main executable, which is not a valid path if the
		// executable was found by a search of the PATH environment
		// variable; In that case, we actually open /proc/self/exe, which
		// is always the actual executable (even if it was deleted/replaced!)
		// but display the path that /proc/self/exe links to.
		symbol_info.dli_fname = "/proc/self/exe";
		return read_symlink("/proc/self/exe");
	} else {
		return symbol_info.dli_fname;
	}
}

#endif

DwarfResolver::FileObject::~FileObject() {
	if(file_handle)
		::close(file_handle);
#ifdef FWK_PLATFORM_LINUX
	if(elf_handle)
		elf_end(elf_handle);
#endif
	if(dwarf_handle)
		dwarf_finish(dwarf_handle, nullptr);
}

void DwarfResolver::resolve(ResolvedTrace &trace) {
	// trace.addr is a virtual address in memory pointing to some code.
	// Let's try to find from which loaded object it comes from.
	// The loaded object can be yourself btw.

	// TODO: figure out which binary file to load...

#ifdef FWK_PLATFORM_LINUX

	Dl_info symbol_info;
	int dladdr_result = 0;
#ifdef LINK_MAP
	link_map *link_map;
	// We request the link map so we can get information about offsets
	dladdr_result =
		dladdr1(trace.addr, &symbol_info, reinterpret_cast<void **>(&link_map), RTLD_DL_LINKMAP);
#else
	// Android doesn't have dladdr1. Don't use the linker map.
	dladdr_result = dladdr(trace.addr, &symbol_info);
#endif
	if(!dladdr_result)
		return; // dat broken trace...

	// Now we get in symbol_info:
	// .dli_fname:
	//      pathname of the shared object that contains the address.
	// .dli_fbase:
	//      where the object is loaded in memory.
	// .dli_sname:
	//      the name of the nearest symbol to trace.addr, we expect a
	//      function name.
	// .dli_saddr:
	//      the exact address corresponding to .dli_sname.
	//
	// And in link_map:
	// .l_addr:
	//      difference between the address in the ELF file and the address
	//      in memory
	// l_name:
	//      absolute pathname where the object was found

	if(symbol_info.dli_sname)
		trace.object_function = demangle(symbol_info.dli_sname);
	if(!symbol_info.dli_fname)
		return;

	trace.object_filename = resolve_exec_path(symbol_info);
	FileObject &fobj = load_object_with_dwarf(symbol_info.dli_fname);
#else
	// TODO: not correct
	auto exec_path = string(executablePath());
	trace.object_filename = exec_path;
	FileObject &fobj = load_object_with_dwarf(exec_path.c_str());
#endif

	if(!fobj.dwarf_handle)
		return; // sad, we couldn't load the object :(

#ifdef LINK_MAP
	// Convert the address to a module relative one by looking at
	// the module's loading address in the link map
	Dwarf_Addr address =
		reinterpret_cast<uintptr_t>(trace.addr) - reinterpret_cast<uintptr_t>(link_map->l_addr);
#else
	Dwarf_Addr address = reinterpret_cast<uintptr_t>(trace.addr);
#endif

	if(trace.object_function.empty()) {
		auto &scache = fobj.symbol_cache;
		auto it = std::lower_bound(begin(scache), end(scache), address,
								   [](auto &pair, auto key) { return pair.first < key; });
		if(it != scache.end() && it->first == address)
			trace.object_function = demangle(it->second);
	}

	// Get the Compilation Unit DIE for the address
	Dwarf_Die die = find_die(fobj, address);

	if(!die)
		return; // this time we lost the game :/

	// libdwarf doesn't give us direct access to its objects, it always
	// allocates a copy for the caller. We keep that copy alive in a cache
	// and we deallocate it later when it's no longer required.
	DieCacheEntry &die_object = get_die_cache(fobj, die);
	if(die_object.isEmpty())
		return; // We have no line section for this DIE

	auto &lsection = die_object.line_section;
	auto it = std::lower_bound(begin(lsection), end(lsection), address,
							   [](auto &arg, auto addr) { return arg.first < addr; });
	if(it == lsection.end())
		return;
	if(it->first != address) {
		if(it == lsection.begin())
			return;
		--it;
	}

	// Get the Dwarf_Line that the address points to and call libdwarf
	// to get source file, line and column info.
	Dwarf_Line line = die_object.line_buffer[it->second];
	Dwarf_Error error = DW_DLE_NE;

	char *filename;
	if(dwarf_linesrc(line, &filename, &error) == DW_DLV_OK) {
		trace.source.filename = string(filename);
		dwarf_dealloc(fobj.dwarf_handle, filename, DW_DLA_STRING);
	}

	Dwarf_Unsigned number = 0;
	if(dwarf_lineno(line, &number, &error) == DW_DLV_OK) {
		trace.source.line = number;
	} else {
		trace.source.line = 0;
	}

	if(dwarf_lineoff_b(line, &number, &error) == DW_DLV_OK) {
		trace.source.col = number;
	} else {
		trace.source.col = 0;
	}

	vector<string> namespace_stack;
	// This is slowest part, is goes through all the dies in CU...
	deep_first_search_by_pc(fobj, die, address, namespace_stack,
							InlinersSearchCB{trace, fobj, die});

	dwarf_dealloc(fobj.dwarf_handle, die, DW_DLA_DIE);
}

static bool cstrings_eq(const char *a, const char *b) {
	if(!a || !b) {
		return false;
	}
	return strcmp(a, b) == 0;
}

#ifdef FWK_PLATFORM_LINUX
template <int bits> bool DwarfResolver::getElfData(FileObject &r, string &debuglink) {
	Elf_Scn *elf_section = 0;
	Elf_Data *elf_data = 0;
	If<bits == 32, Elf32_Shdr, Elf64_Shdr> *section_header = 0;
	Elf_Scn *symbol_section = 0;
	size_t symbol_count = 0;
	size_t symbol_strings = 0;
	If<bits == 32, Elf32_Sym, Elf64_Sym> *symbol = 0;
	const char *section_name = 0;

	// Get the number of sections
	// We use the new APIs as elf_getshnum is deprecated
	size_t shdrnum = 0;
	if(elf_getshdrnum(r.elf_handle, &shdrnum) == -1)
		return false;

	// Get the index to the string section
	size_t shdrstrndx = 0;
	if(elf_getshdrstrndx(r.elf_handle, &shdrstrndx) == -1)
		return false;

	while((elf_section = elf_nextscn(r.elf_handle, elf_section)) != NULL) {
		if constexpr(bits == 32)
			section_header = elf32_getshdr(elf_section);
		else
			section_header = elf64_getshdr(elf_section);

		if(section_header == NULL)
			return false;

		if((section_name = elf_strptr(r.elf_handle, shdrstrndx, section_header->sh_name)) == NULL)
			return false;

		if(cstrings_eq(section_name, ".gnu_debuglink")) {
			elf_data = elf_getdata(elf_section, NULL);
			if(elf_data && elf_data->d_size > 0) {
				debuglink = string(reinterpret_cast<const char *>(elf_data->d_buf));
			}
		}

		switch(section_header->sh_type) {
		case SHT_SYMTAB:
			symbol_section = elf_section;
			symbol_count = section_header->sh_size / section_header->sh_entsize;
			symbol_strings = section_header->sh_link;
			break;

			/* We use .dynsyms as a last resort, we prefer .symtab */
		case SHT_DYNSYM:
			if(!symbol_section) {
				symbol_section = elf_section;
				symbol_count = section_header->sh_size / section_header->sh_entsize;
				symbol_strings = section_header->sh_link;
			}
			break;
		}
	}

	if(symbol_section && symbol_count && symbol_strings) {
		elf_data = elf_getdata(symbol_section, NULL);
		using Sym = If<bits == 32, Elf32_Sym, Elf64_Sym>;
		symbol = reinterpret_cast<Sym *>(elf_data->d_buf);
		for(size_t i = 0; i < symbol_count; ++i) {
			int type = bits == 32 ? ELF32_ST_TYPE(symbol->st_info) : ELF64_ST_TYPE(symbol->st_info);
			if(type == STT_FUNC && symbol->st_value > 0)
				r.symbol_cache.emplace_back(
					symbol->st_value, elf_strptr(r.elf_handle, symbol_strings, symbol->st_name));
			++symbol;
		}
	}
	return true;
}
#endif

DwarfResolver::FileObject &DwarfResolver::load_object_with_dwarf(const string &filename_object) {
	if(!dwarf_loaded) {
		// Set the ELF library operating version
		// If that fails there's nothing we can do
#ifdef FWK_PLATFORM_LINUX
		dwarf_loaded = elf_version(EV_CURRENT) != EV_NONE;
#else
		dwarf_loaded = true;
#endif
	}

	for(int n = 0; n < file_objects.size(); n++)
		if(file_names[n] == filename_object)
			return *file_objects[n];
	file_names.emplace_back(filename_object);
	file_objects.emplace_back();
	file_objects.back().emplace();

	// this new object is empty for now
	auto &r = *file_objects.back();

	r.file_handle = open(filename_object.c_str(), O_RDONLY);
	if(r.file_handle < 0)
		return r;

#ifdef FWK_PLATFORM_LINUX
	// Try to get an ELF handle. We need to read the ELF sections
	// because we want to see if there is a .gnu_debuglink section
	// that points to a split debug file
	r.elf_handle = elf_begin(r.file_handle, ELF_C_READ, NULL);
	if(!r.elf_handle)
		return r;

	const char *e_ident = elf_getident(r.elf_handle, 0);
	if(!e_ident)
		return r;

	string debuglink;
	// Iterate through the ELF sections to try to get a gnu_debuglink
	// note and also to cache the symbol table.
	if(e_ident[EI_CLASS] == ELFCLASS32) {
		if(!getElfData<32>(r, debuglink))
			return r;
	} else if(e_ident[EI_CLASS] == ELFCLASS64) {
		// libelf might have been built without 64 bit support
#if __LIBELF64
		if(!getElfData<64>(r, debuglink))
			return r;
#endif
	}

	makeSorted(r.symbol_cache);

	if(!debuglink.empty()) {
		// We have a debuglink section! Open an elf instance on that
		// file instead. If we can't open the file, then return
		// the elf handle we had already opened.
		auto debuglink_file = open(debuglink.c_str(), O_RDONLY);
		if(debuglink_file > 0) {
			auto debuglink_elf = elf_begin(debuglink_file, ELF_C_READ, NULL);

			// If we have a valid elf handle, return the new elf handle
			// and file handle and discard the original ones
			if(debuglink_elf) {
				elf_end(r.elf_handle);
				close(r.file_handle);
				r.elf_handle = debuglink_elf;
				r.file_handle = debuglink_file;
			} else
				close(debuglink_file);
		}
	}

	// Ok, we have a valid ELF handle, let's try to get debug symbols
	Dwarf_Error error = DW_DLE_NE;
	auto result = dwarf_elf_init(r.elf_handle, DW_DLC_READ, 0, NULL, &r.dwarf_handle, &error);
#else
	Dwarf_Error error = DW_DLE_NE;
	//auto result = dwarf_init_b(r.file_handle, DW_DLC_READ, 0, 0, NULL, &r.dwarf_handle, &error);
	auto result = dwarf_init_path(filename_object.c_str(), 0, 0, DW_DLC_READ, 0, 0, 0,
								  &r.dwarf_handle, 0, 0, 0, &error);
	printf("Error: %s\n", dwarf_errmsg(error));
#endif

	// We don't do any special handling for DW_DLV_NO_ENTRY specially.
	// If we get an error, or the file doesn't have debug information
	// we just return.
	if(result != DW_DLV_OK)
		return r; // TODO: Nice error handling...
	return r;
}

DwarfResolver::DieCacheEntry &DwarfResolver::get_die_cache(FileObject &fobj, Dwarf_Die die) {
	Dwarf_Error error = DW_DLE_NE;

	// Get the die offset, we use it as the cache key
	Dwarf_Off die_offset;
	if(dwarf_dieoffset(die, &die_offset, &error) != DW_DLV_OK) {
		die_offset = 0;
	}

	DieCacheEntry *entry = nullptr;
	for(int n = 0; n < fobj.die_cache.size(); n++)
		if(fobj.die_offsets[n] == die_offset) {
			entry = fobj.die_cache[n].get();
			break;
		}
	if(!entry) {
		fobj.die_offsets.emplace_back(die_offset);
		fobj.die_cache.emplace_back();
		fobj.die_cache.back().emplace();
		entry = fobj.die_cache.back().get();
	}
	auto &de = *entry;
	fobj.current_cu = &de;

	Dwarf_Addr line_addr;
	Dwarf_Small table_count;

	// The addresses in the line section are not fully sorted (they might
	// be sorted by block of code belonging to the same file), which makes
	// it necessary to do so before searching is possible.
	//
	// As libdwarf allocates a copy of everything, let's get the contents
	// of the line section and keep it around. We also create a map of
	// program counter to line table indices so we can search by address
	// and get the line buffer index.
	//
	// To make things more difficult, the same address can span more than
	// one line.

	// Get the line context for the DIE
	if(dwarf_srclines_b(die, 0, &table_count, &de.line_context, &error) == DW_DLV_OK) {
		// Get the source lines for this line context, to be deallocated
		// later
		if(dwarf_srclines_from_linecontext(de.line_context, &de.line_buffer, &de.line_count,
										   &error) == DW_DLV_OK) {

			// Add all the addresses to our map
			for(int i = 0; i < de.line_count; i++) {
				if(dwarf_lineaddr(de.line_buffer[i], &line_addr, &error) != DW_DLV_OK)
					line_addr = 0;
				de.line_section.emplace_back(line_addr, i);
			}
			makeSorted(de.line_section);
		}
	}

	// For each CU, cache the function DIEs that contain the
	// DW_AT_specification attribute. When building with -g3 the function
	// DIEs are separated in declaration and specification, with the
	// declaration containing only the name and parameters and the
	// specification the low/high pc and other compiler attributes.
	//
	// We cache those specifications so we don't skip over the declarations,
	// because they have no pc, and we can do namespace resolution for
	// DWARF function names.
	Dwarf_Debug dwarf = fobj.dwarf_handle;
	Dwarf_Die current_die = 0;
	if(dwarf_child(die, &current_die, &error) == DW_DLV_OK) {
		for(;;) {
			Dwarf_Die sibling_die = 0;

			Dwarf_Half tag_value;
			dwarf_tag(current_die, &tag_value, &error);

			if(tag_value == DW_TAG_subprogram || tag_value == DW_TAG_inlined_subroutine) {
				Dwarf_Bool has_attr = 0;
				if(dwarf_hasattr(current_die, DW_AT_specification, &has_attr, &error) ==
				   DW_DLV_OK) {
					if(has_attr) {
						Dwarf_Attribute attr_mem;
						if(dwarf_attr(current_die, DW_AT_specification, &attr_mem, &error) ==
						   DW_DLV_OK) {
							Dwarf_Off spec_offset = 0;
							if(dwarf_formref(attr_mem, &spec_offset, &error) == DW_DLV_OK) {
								Dwarf_Off spec_die_offset;
								if(dwarf_dieoffset(current_die, &spec_die_offset, &error) ==
								   DW_DLV_OK)
									de.spec_section.emplace_back(spec_offset, spec_die_offset);
							}
						}
						dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
					}
				}
			}

			int result = dwarf_siblingof(dwarf, current_die, &sibling_die, &error);
			if(isOneOf(result, DW_DLV_ERROR, DW_DLV_NO_ENTRY))
				break;

			if(current_die != die) {
				dwarf_dealloc(dwarf, current_die, DW_DLA_DIE);
				current_die = 0;
			}

			current_die = sibling_die;
		}
	}

	makeSorted(de.spec_section);
	return de;
}

Dwarf_Die DwarfResolver::get_referenced_die(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Half attr,
											bool global) {
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Attribute attr_mem;

	Dwarf_Die found_die = NULL;
	if(dwarf_attr(die, attr, &attr_mem, &error) == DW_DLV_OK) {
		Dwarf_Off offset;
		int result = global ? dwarf_global_formref(attr_mem, &offset, &error) :
							  dwarf_formref(attr_mem, &offset, &error);

		if(result == DW_DLV_OK)
			if(dwarf_offdie(dwarf, offset, &found_die, &error) != DW_DLV_OK)
				found_die = NULL;
		dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
	}
	return found_die;
}

string DwarfResolver::get_referenced_die_name(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Half attr,
											  bool global) {
	Dwarf_Error error = DW_DLE_NE;
	string value;

	Dwarf_Die found_die = get_referenced_die(dwarf, die, attr, global);

	if(found_die) {
		char *name;
		if(dwarf_diename(found_die, &name, &error) == DW_DLV_OK) {
			if(name)
				value = name;
			dwarf_dealloc(dwarf, name, DW_DLA_STRING);
		}
		dwarf_dealloc(dwarf, found_die, DW_DLA_DIE);
	}

	return value;
}

// Returns a spec DIE linked to the passed one. The caller should
// deallocate the DIE
Dwarf_Die DwarfResolver::get_spec_die(FileObject &fobj, Dwarf_Die die) {
	Dwarf_Debug dwarf = fobj.dwarf_handle;
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Off die_offset;
	if(fobj.current_cu && dwarf_die_CU_offset(die, &die_offset, &error) == DW_DLV_OK) {
		auto &ssection = fobj.current_cu->spec_section;
		auto it = std::lower_bound(begin(ssection), end(ssection), die_offset,
								   [](auto &pair, auto key) { return pair.first < key; });

		// If we have a DIE that completes the current one, check if
		// that one has the pc we are looking for
		if(it != ssection.end() && it->first == die_offset) {
			Dwarf_Die spec_die = 0;
			if(dwarf_offdie(dwarf, it->second, &spec_die, &error) == DW_DLV_OK) {
				return spec_die;
			}
		}
	}

	// Maybe we have an abstract origin DIE with the function information?
	return get_referenced_die(fobj.dwarf_handle, die, DW_AT_abstract_origin, true);
}

bool DwarfResolver::die_has_pc(FileObject &fobj, Dwarf_Die die, Dwarf_Addr pc) {
	Dwarf_Addr low_pc = 0, high_pc = 0;
	Dwarf_Half high_pc_form = 0;
	Dwarf_Form_Class return_class;
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Debug dwarf = fobj.dwarf_handle;
	bool has_lowpc = false;
	bool has_highpc = false;
	bool has_ranges = false;

	if(dwarf_lowpc(die, &low_pc, &error) == DW_DLV_OK) {
		// If we have a low_pc check if there is a high pc.
		// If we don't have a high pc this might mean we have a base
		// address for the ranges list or just an address.
		has_lowpc = true;

		if(dwarf_highpc_b(die, &high_pc, &high_pc_form, &return_class, &error) == DW_DLV_OK) {
			// We do have a high pc. In DWARF 4+ this is an offset from the
			// low pc, but in earlier versions it's an absolute address.

			has_highpc = true;
			// In DWARF 2/3 this would be a DW_FORM_CLASS_ADDRESS
			if(return_class == DW_FORM_CLASS_CONSTANT) {
				high_pc = low_pc + high_pc;
			}

			// We have low and high pc, check if our address
			// is in that range
			return pc >= low_pc && pc < high_pc;
		}
	} else {
		// Reset the low_pc, in case dwarf_lowpc failing set it to some
		// undefined value.
		low_pc = 0;
	}

	// Check if DW_AT_ranges is present and search for the PC in the
	// returned ranges list. We always add the low_pc, as it not set it will
	// be 0, in case we had a DW_AT_low_pc and DW_AT_ranges pair
	bool result = false;

	Dwarf_Attribute attr;
	if(dwarf_attr(die, DW_AT_ranges, &attr, &error) == DW_DLV_OK) {

		Dwarf_Off offset;
		if(dwarf_global_formref(attr, &offset, &error) == DW_DLV_OK) {
			Dwarf_Ranges *ranges;
			Dwarf_Signed ranges_count = 0;
			Dwarf_Unsigned byte_count = 0;

			if(dwarf_get_ranges_a(dwarf, offset, die, &ranges, &ranges_count, &byte_count,
								  &error) == DW_DLV_OK) {
				has_ranges = ranges_count != 0;
				for(int i = 0; i < ranges_count; i++) {
					if(ranges[i].dwr_addr1 != 0 && pc >= ranges[i].dwr_addr1 + low_pc &&
					   pc < ranges[i].dwr_addr2 + low_pc) {
						result = true;
						break;
					}
				}
				dwarf_ranges_dealloc(dwarf, ranges, ranges_count);
			}
		}
	}

	// Last attempt. We might have a single address set as low_pc.
	if(!result && low_pc != 0 && pc == low_pc) {
		result = true;
	}

	// If we don't have lowpc, highpc and ranges maybe this DIE is a
	// declaration that relies on a DW_AT_specification DIE that happens
	// later. Use the specification cache we filled when we loaded this CU.
	if(!result && (!has_lowpc && !has_highpc && !has_ranges)) {
		Dwarf_Die spec_die = get_spec_die(fobj, die);
		if(spec_die) {
			result = die_has_pc(fobj, spec_die, pc);
			dwarf_dealloc(dwarf, spec_die, DW_DLA_DIE);
		}
	}

	return result;
}

void DwarfResolver::get_type(Dwarf_Debug dwarf, Dwarf_Die die, string &type) {
	Dwarf_Error error = DW_DLE_NE;

	Dwarf_Die child = 0;
	if(dwarf_child(die, &child, &error) == DW_DLV_OK) {
		get_type(dwarf, child, type);
	}

	if(child) {
		type.insert(0, "::");
		dwarf_dealloc(dwarf, child, DW_DLA_DIE);
	}

	char *name;
	if(dwarf_diename(die, &name, &error) == DW_DLV_OK) {
		type.insert(0, string(name));
		dwarf_dealloc(dwarf, name, DW_DLA_STRING);
	} else {
		type.insert(0, "<unknown>");
	}
}

string DwarfResolver::get_type_by_signature(Dwarf_Debug dwarf, Dwarf_Die die) {
	Dwarf_Error error = DW_DLE_NE;

	Dwarf_Sig8 signature;
	Dwarf_Bool has_attr = 0;
	if(dwarf_hasattr(die, DW_AT_signature, &has_attr, &error) == DW_DLV_OK) {
		if(has_attr) {
			Dwarf_Attribute attr_mem;
			if(dwarf_attr(die, DW_AT_signature, &attr_mem, &error) == DW_DLV_OK) {
				if(dwarf_formsig8(attr_mem, &signature, &error) != DW_DLV_OK) {
					return string("<no type signature>");
				}
			}
			dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
		}
	}

	Dwarf_Unsigned next_cu_header;
	Dwarf_Sig8 tu_signature;
	string result;
	bool found = false;

	while(dwarf_next_cu_header_d(dwarf, 0, 0, 0, 0, 0, 0, 0, &tu_signature, 0, &next_cu_header, 0,
								 &error) == DW_DLV_OK) {

		if(strncmp(signature.signature, tu_signature.signature, 8) == 0) {
			Dwarf_Die type_cu_die = 0;
			if(dwarf_siblingof_b(dwarf, 0, 0, &type_cu_die, &error) == DW_DLV_OK) {
				Dwarf_Die child_die = 0;
				if(dwarf_child(type_cu_die, &child_die, &error) == DW_DLV_OK) {
					get_type(dwarf, child_die, result);
					found = !result.empty();
					dwarf_dealloc(dwarf, child_die, DW_DLA_DIE);
				}
				dwarf_dealloc(dwarf, type_cu_die, DW_DLA_DIE);
			}
		}
	}

	if(found) {
		while(dwarf_next_cu_header_d(dwarf, 0, 0, 0, 0, 0, 0, 0, 0, 0, &next_cu_header, 0,
									 &error) == DW_DLV_OK) {
			// Reset the cu header state. Unfortunately, libdwarf's
			// next_cu_header API keeps its own iterator per Dwarf_Debug
			// that can't be reset. We need to keep fetching elements until
			// the end.
		}
	} else {
		result = "UNKNOWN_SIG";
		/* // If we couldn't resolve the type just print out the signature
		std::ostringstream string_stream;
		string_stream << "<0x" << std::hex << std::setfill('0');
		for(int i = 0; i < 8; ++i) {
			string_stream << std::setw(2) << std::hex
						  << (int)(unsigned char)(signature.signature[i]);
		}
		string_stream << ">";
		result = string_stream.str();*/
	}
	return result;
}

// Types are resolved from right to left: we get the variable name first
// and then all specifiers (like const or pointer) in a chain of DW_AT_type
// DIEs. Call this function recursively until we get a complete type
// string.
void DwarfResolver::set_parameter_string(FileObject &fobj, Dwarf_Die die, type_context_t &context) {
	char *name;
	Dwarf_Error error = DW_DLE_NE;

	// typedefs contain also the base type, so we skip it and only
	// print the typedef name
	if(!context.is_typedef) {
		if(dwarf_diename(die, &name, &error) == DW_DLV_OK) {
			if(!context.text.empty()) {
				context.text.insert(0, " ");
			}
			context.text.insert(0, string(name));
			dwarf_dealloc(fobj.dwarf_handle, name, DW_DLA_STRING);
		}
	} else {
		context.is_typedef = false;
		context.has_type = true;
		if(context.is_const) {
			context.text.insert(0, "const ");
			context.is_const = false;
		}
	}

	bool next_type_is_const = false;
	bool is_keyword = true;

	Dwarf_Half tag = 0;
	Dwarf_Bool has_attr = 0;
	if(dwarf_tag(die, &tag, &error) == DW_DLV_OK) {
		switch(tag) {
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
		case DW_TAG_class_type:
		case DW_TAG_enumeration_type:
			context.has_type = true;
			if(dwarf_hasattr(die, DW_AT_signature, &has_attr, &error) == DW_DLV_OK) {
				// If we have a signature it means the type is defined
				// in .debug_types, so we need to load the DIE pointed
				// at by the signature and resolve it
				if(has_attr) {
					string type = get_type_by_signature(fobj.dwarf_handle, die);
					if(context.is_const)
						type.insert(0, "const ");

					if(!context.text.empty())
						context.text.insert(0, " ");
					context.text.insert(0, type);
				}

				// Treat enums like typedefs, and skip printing its
				// base type
				context.is_typedef = (tag == DW_TAG_enumeration_type);
			}
			break;
		case DW_TAG_const_type:
			next_type_is_const = true;
			break;
		case DW_TAG_pointer_type:
			context.text.insert(0, "*");
			break;
		case DW_TAG_reference_type:
			context.text.insert(0, "&");
			break;
		case DW_TAG_restrict_type:
			context.text.insert(0, "restrict ");
			break;
		case DW_TAG_rvalue_reference_type:
			context.text.insert(0, "&&");
			break;
		case DW_TAG_volatile_type:
			context.text.insert(0, "volatile ");
			break;
		case DW_TAG_typedef:
			// Propagate the const-ness to the next type
			// as typedefs are linked to its base type
			next_type_is_const = context.is_const;
			context.is_typedef = true;
			context.has_type = true;
			break;
		case DW_TAG_base_type:
			context.has_type = true;
			break;
		case DW_TAG_formal_parameter:
			context.has_name = true;
			break;
		default:
			is_keyword = false;
			break;
		}
	}

	if(!is_keyword && context.is_const) {
		context.text.insert(0, "const ");
	}

	context.is_const = next_type_is_const;

	Dwarf_Die ref = get_referenced_die(fobj.dwarf_handle, die, DW_AT_type, true);
	if(ref) {
		set_parameter_string(fobj, ref, context);
		dwarf_dealloc(fobj.dwarf_handle, ref, DW_DLA_DIE);
	}

	if(!context.has_type && context.has_name) {
		context.text.insert(0, "void ");
		context.has_type = true;
	}
}

// Resolve the function return type and parameters
void DwarfResolver::set_function_parameters(string &function_name, vector<string> &ns,
											FileObject &fobj, Dwarf_Die die) {
	Dwarf_Debug dwarf = fobj.dwarf_handle;
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Die current_die = 0;
	string parameters;
	bool has_spec = true;
	// Check if we have a spec DIE. If we do we use it as it contains
	// more information, like parameter names.
	Dwarf_Die spec_die = get_spec_die(fobj, die);
	if(!spec_die) {
		has_spec = false;
		spec_die = die;
	}

	auto it = ns.begin();
	string ns_name;
	for(it = ns.begin(); it < ns.end(); ++it) {
		ns_name.append(*it).append("::");
	}

	if(!ns_name.empty()) {
		function_name.insert(0, ns_name);
	}

	// See if we have a function return type. It can be either on the
	// current die or in its spec one (usually true for inlined functions)
	string return_type = get_referenced_die_name(dwarf, die, DW_AT_type, true);
	if(return_type.empty()) {
		return_type = get_referenced_die_name(dwarf, spec_die, DW_AT_type, true);
	}
	if(!return_type.empty()) {
		return_type.append(" ");
		function_name.insert(0, return_type);
	}

	if(dwarf_child(spec_die, &current_die, &error) == DW_DLV_OK) {
		for(;;) {
			Dwarf_Die sibling_die = 0;

			Dwarf_Half tag_value;
			dwarf_tag(current_die, &tag_value, &error);

			if(tag_value == DW_TAG_formal_parameter) {
				// Ignore artificial (ie, compiler generated) parameters
				bool is_artificial = false;
				Dwarf_Attribute attr_mem;
				if(dwarf_attr(current_die, DW_AT_artificial, &attr_mem, &error) == DW_DLV_OK) {
					Dwarf_Bool flag = 0;
					if(dwarf_formflag(attr_mem, &flag, &error) == DW_DLV_OK) {
						is_artificial = flag != 0;
					}
					dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
				}

				if(!is_artificial) {
					type_context_t context;
					set_parameter_string(fobj, current_die, context);

					if(parameters.empty())
						parameters.append("(");
					else
						parameters.append(", ");

					parameters.append(context.text);
				}
			}

			int result = dwarf_siblingof(dwarf, current_die, &sibling_die, &error);
			if(result == DW_DLV_ERROR) {
				break;
			} else if(result == DW_DLV_NO_ENTRY) {
				break;
			}

			if(current_die != die) {
				dwarf_dealloc(dwarf, current_die, DW_DLA_DIE);
				current_die = 0;
			}

			current_die = sibling_die;
		}
	}
	if(parameters.empty())
		parameters = "(";
	parameters.append(")");

	// If we got a spec DIE we need to deallocate it
	if(has_spec)
		dwarf_dealloc(dwarf, spec_die, DW_DLA_DIE);

	function_name.append(parameters);
}

void DwarfResolver::InlinersSearchCB::operator()(Dwarf_Die die, vector<string> &ns) {
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Half tag_value;
	Dwarf_Attribute attr_mem;
	Dwarf_Debug dwarf = fobj.dwarf_handle;

	dwarf_tag(die, &tag_value, &error);

	switch(tag_value) {
		char *name;
	case DW_TAG_subprogram:
		if(!trace.source.function.empty())
			break;
		if(dwarf_diename(die, &name, &error) == DW_DLV_OK) {
			trace.source.function = string(name);
			dwarf_dealloc(dwarf, name, DW_DLA_STRING);
		} else {
			// We don't have a function name in this DIE.
			// Check if there is a referenced non-defining
			// declaration.
			trace.source.function =
				get_referenced_die_name(dwarf, die, DW_AT_abstract_origin, true);
			if(trace.source.function.empty()) {
				trace.source.function =
					get_referenced_die_name(dwarf, die, DW_AT_specification, true);
			}
		}

		// Append the function parameters, if available
		set_function_parameters(trace.source.function, ns, fobj, die);

		// If the object function name is empty, it's possible that
		// there is no dynamic symbol table (maybe the executable
		// was stripped or not built with -rdynamic). See if we have
		// a DWARF linkage name to use instead. We try both
		// linkage_name and MIPS_linkage_name because the MIPS tag
		// was the unofficial one until it was adopted in DWARF4.
		// Old gcc versions generate MIPS_linkage_name
		if(trace.object_function.empty()) {
			if(dwarf_attr(die, DW_AT_linkage_name, &attr_mem, &error) != DW_DLV_OK)
				if(dwarf_attr(die, DW_AT_MIPS_linkage_name, &attr_mem, &error) != DW_DLV_OK)
					break;

			char *linkage;
			if(dwarf_formstring(attr_mem, &linkage, &error) == DW_DLV_OK) {
				trace.object_function = fwk::demangle(linkage);
				dwarf_dealloc(dwarf, linkage, DW_DLA_STRING);
			}
			dwarf_dealloc(dwarf, name, DW_DLA_ATTR);
		}
		break;

	case DW_TAG_inlined_subroutine:
		ResolvedTrace::SourceLoc sloc;

		if(dwarf_diename(die, &name, &error) == DW_DLV_OK) {
			sloc.function = string(name);
			dwarf_dealloc(dwarf, name, DW_DLA_STRING);
		} else {
			// We don't have a name for this inlined DIE, it could
			// be that there is an abstract origin instead.
			// Get the DW_AT_abstract_origin value, which is a
			// reference to the source DIE and try to get its name
			sloc.function = get_referenced_die_name(dwarf, die, DW_AT_abstract_origin, true);
		}

		set_function_parameters(sloc.function, ns, fobj, die);

		string file = die_call_file(dwarf, die, cu_die);
		if(!file.empty())
			sloc.filename = file;

		Dwarf_Unsigned number = 0;
		if(dwarf_attr(die, DW_AT_call_line, &attr_mem, &error) == DW_DLV_OK) {
			if(dwarf_formudata(attr_mem, &number, &error) == DW_DLV_OK)
				sloc.line = number;
			dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
		}

		if(dwarf_attr(die, DW_AT_call_column, &attr_mem, &error) == DW_DLV_OK) {
			if(dwarf_formudata(attr_mem, &number, &error) == DW_DLV_OK)
				sloc.col = number;
			dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
		}

		trace.inliners.push_back(sloc);
		break;
	};
}

Dwarf_Die DwarfResolver::find_fundie_by_pc(FileObject &fobj, Dwarf_Die parent_die, Dwarf_Addr pc,
										   Dwarf_Die result) {
	Dwarf_Die current_die = 0;
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Debug dwarf = fobj.dwarf_handle;

	if(dwarf_child(parent_die, &current_die, &error) != DW_DLV_OK) {
		return NULL;
	}

	for(;;) {
		Dwarf_Die sibling_die = 0;
		Dwarf_Half tag_value;
		dwarf_tag(current_die, &tag_value, &error);

		switch(tag_value) {
		case DW_TAG_subprogram:
		case DW_TAG_inlined_subroutine:
			if(die_has_pc(fobj, current_die, pc)) {
				return current_die;
			}
		};
		bool declaration = false;
		Dwarf_Attribute attr_mem;
		if(dwarf_attr(current_die, DW_AT_declaration, &attr_mem, &error) == DW_DLV_OK) {
			Dwarf_Bool flag = 0;
			if(dwarf_formflag(attr_mem, &flag, &error) == DW_DLV_OK) {
				declaration = flag != 0;
			}
			dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
		}

		if(!declaration) {
			// let's be curious and look deeper in the tree, functions are
			// not necessarily at the first level, but might be nested
			// inside a namespace, structure, a function, an inlined
			// function etc.
			Dwarf_Die die_mem = 0;
			Dwarf_Die indie = find_fundie_by_pc(fobj, current_die, pc, die_mem);
			if(indie) {
				result = die_mem;
				return result;
			}
		}

		int res = dwarf_siblingof(dwarf, current_die, &sibling_die, &error);
		if(res == DW_DLV_ERROR) {
			return NULL;
		} else if(res == DW_DLV_NO_ENTRY) {
			break;
		}

		if(current_die != parent_die) {
			dwarf_dealloc(dwarf, current_die, DW_DLA_DIE);
			current_die = 0;
		}

		current_die = sibling_die;
	}
	return NULL;
}

template <typename CB>
bool DwarfResolver::deep_first_search_by_pc(FileObject &fobj, Dwarf_Die parent_die, Dwarf_Addr pc,
											vector<string> &ns, CB cb) {
	Dwarf_Die current_die = 0;
	Dwarf_Debug dwarf = fobj.dwarf_handle;
	Dwarf_Error error = DW_DLE_NE;

	if(dwarf_child(parent_die, &current_die, &error) != DW_DLV_OK) {
		return false;
	}

	bool branch_has_pc = false;
	bool has_namespace = false;
	for(;;) {
		Dwarf_Die sibling_die = 0;

		Dwarf_Half tag;
		if(dwarf_tag(current_die, &tag, &error) == DW_DLV_OK) {
			if(tag == DW_TAG_namespace || tag == DW_TAG_class_type) {
				char *ns_name = NULL;
				if(dwarf_diename(current_die, &ns_name, &error) == DW_DLV_OK) {
					if(ns_name)
						ns.push_back(string(ns_name));
					else
						ns.push_back("<unknown>");

					dwarf_dealloc(dwarf, ns_name, DW_DLA_STRING);
				} else
					ns.push_back("<unknown>");

				has_namespace = true;
			}
		}

		bool declaration = false;
		Dwarf_Attribute attr_mem;
		if(tag != DW_TAG_class_type &&
		   dwarf_attr(current_die, DW_AT_declaration, &attr_mem, &error) == DW_DLV_OK) {
			Dwarf_Bool flag = 0;
			if(dwarf_formflag(attr_mem, &flag, &error) == DW_DLV_OK) {
				declaration = flag != 0;
			}
			dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);
		}

		if(!declaration) {
			// let's be curious and look deeper in the tree, function are
			// not necessarily at the first level, but might be nested
			// inside a namespace, structure, a function, an inlined
			// function etc.
			branch_has_pc = deep_first_search_by_pc(fobj, current_die, pc, ns, cb);
		}

		if(!branch_has_pc)
			branch_has_pc = die_has_pc(fobj, current_die, pc);

		if(branch_has_pc)
			cb(current_die, ns);

		int result = dwarf_siblingof(dwarf, current_die, &sibling_die, &error);
		if(result == DW_DLV_ERROR)
			return false;
		else if(result == DW_DLV_NO_ENTRY)
			break;

		if(current_die != parent_die) {
			dwarf_dealloc(dwarf, current_die, DW_DLA_DIE);
			current_die = 0;
		}

		if(has_namespace) {
			has_namespace = false;
			ns.pop_back();
		}
		current_die = sibling_die;
	}

	if(has_namespace)
		ns.pop_back();
	return branch_has_pc;
}

string DwarfResolver::die_call_file(Dwarf_Debug dwarf, Dwarf_Die die, Dwarf_Die cu_die) {
	Dwarf_Attribute attr_mem;
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Signed file_index;

	string file;

	if(dwarf_attr(die, DW_AT_call_file, &attr_mem, &error) == DW_DLV_OK) {
		if(dwarf_formsdata(attr_mem, &file_index, &error) != DW_DLV_OK) {
			file_index = 0;
		}
		dwarf_dealloc(dwarf, attr_mem, DW_DLA_ATTR);

		if(file_index == 0)
			return file;

		char **srcfiles = 0;
		Dwarf_Signed file_count = 0;
		if(dwarf_srcfiles(cu_die, &srcfiles, &file_count, &error) == DW_DLV_OK) {
			// TODO: why is it <= 0 sometimes?
			if(file_index >= 1 && file_index <= file_count)
				file = string(srcfiles[file_index - 1]);

			// Deallocate all strings!
			for(int i = 0; i < file_count; ++i) {
				dwarf_dealloc(dwarf, srcfiles[i], DW_DLA_STRING);
			}
			dwarf_dealloc(dwarf, srcfiles, DW_DLA_LIST);
		}
	}
	return file;
}

Dwarf_Die DwarfResolver::find_die(FileObject &fobj, Dwarf_Addr addr) {
	// Let's get to work! First see if we have a debug_aranges section so
	// we can speed up the search

	Dwarf_Debug dwarf = fobj.dwarf_handle;
	Dwarf_Error error = DW_DLE_NE;
	Dwarf_Arange *aranges;
	Dwarf_Signed arange_count;

	Dwarf_Die returnDie;
	bool found = false;
	if(dwarf_get_aranges(dwarf, &aranges, &arange_count, &error) != DW_DLV_OK) {
		aranges = NULL;
	}

	if(aranges) {
		// We have aranges. Get the one where our address is.
		Dwarf_Arange arange;
		if(dwarf_get_arange(aranges, arange_count, addr, &arange, &error) == DW_DLV_OK) {

			// We found our address. Get the compilation-unit DIE offset
			// represented by the given address range.
			Dwarf_Off cu_die_offset;
			if(dwarf_get_cu_die_offset(arange, &cu_die_offset, &error) == DW_DLV_OK) {
				// Get the DIE at the offset returned by the aranges search.
				// We set is_info to 1 to specify that the offset is from
				// the .debug_info section (and not .debug_types)
				int dwarf_result = dwarf_offdie_b(dwarf, cu_die_offset, 1, &returnDie, &error);

				found = dwarf_result == DW_DLV_OK;
			}
			dwarf_dealloc(dwarf, arange, DW_DLA_ARANGE);
		}
	}

	if(found)
		return returnDie; // The caller is responsible for freeing the die

	// The search for aranges failed. Try to find our address by scanning
	// all compilation units.
	Dwarf_Unsigned next_cu_header;
	Dwarf_Half tag = 0;
	returnDie = 0;

	while(!found && dwarf_next_cu_header_d(dwarf, 1, 0, 0, 0, 0, 0, 0, 0, 0, &next_cu_header, 0,
										   &error) == DW_DLV_OK) {

		if(returnDie)
			dwarf_dealloc(dwarf, returnDie, DW_DLA_DIE);

		if(dwarf_siblingof(dwarf, 0, &returnDie, &error) == DW_DLV_OK) {
			if((dwarf_tag(returnDie, &tag, &error) == DW_DLV_OK) && tag == DW_TAG_compile_unit) {
				if(die_has_pc(fobj, returnDie, addr)) {
					found = true;
				}
			}
		}
	}

	if(found) {
		while(dwarf_next_cu_header_d(dwarf, 1, 0, 0, 0, 0, 0, 0, 0, 0, &next_cu_header, 0,
									 &error) == DW_DLV_OK) {
			// Reset the cu header state. Libdwarf's next_cu_header API
			// keeps its own iterator per Dwarf_Debug that can't be reset.
			// We need to keep fetching elements until the end.
		}
	}

	if(found)
		return returnDie;

	// We couldn't find any compilation units with ranges or a high/low pc.
	// Try again by looking at all DIEs in all compilation units.
	Dwarf_Die cudie;
	while(dwarf_next_cu_header_d(dwarf, 1, 0, 0, 0, 0, 0, 0, 0, 0, &next_cu_header, 0, &error) ==
		  DW_DLV_OK) {
		if(dwarf_siblingof(dwarf, 0, &cudie, &error) == DW_DLV_OK) {
			Dwarf_Die die_mem = 0;
			Dwarf_Die resultDie = find_fundie_by_pc(fobj, cudie, addr, die_mem);

			if(resultDie) {
				found = true;
				break;
			}
		}
	}

	if(found) {
		while(dwarf_next_cu_header_d(dwarf, 1, 0, 0, 0, 0, 0, 0, 0, 0, &next_cu_header, 0,
									 &error) == DW_DLV_OK) {
			// Reset the cu header state. Libdwarf's next_cu_header API
			// keeps its own iterator per Dwarf_Debug that can't be reset.
			// We need to keep fetching elements until the end.
		}
	}

	if(found)
		return cudie;

	// We failed.
	return NULL;
}
}
