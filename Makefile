# Main Options:
#   PLATFORM:      linux, mingw or html
#   MODE:          release or debug
#   COMPILER:      name of the compiler; clang++ or g++ is used by default
#   LINKER:        name of linker; by default same as compiler
#   ADD_FLAGS:     additional compiler flags
#   STATS:         gathers build times for each object file
#   SPLIT_MODULES: normally modules composed of multiple .cpp files are built together; this
#                  option forces separate compilation for each .cpp file
#   MINGW_PREFIX:  prefix for mingw toolset; Can be set in the environment

# Conditional compilation:
#   IMGUI: enabled or disabled (by default if imgui is present, it's enabled)
#   GEOM:  enabled or disabled (enabled by default)

# Special targets:
#  print-variables: prints contents of main variables
#  print-stats:     prints build stats (generated with STATS option)
#  clean-checker:   removes build files for checker
#  clean-all:       clears all possible targets (including checker)

# TODO: co zrobić z paranoid ? dodac jako opcję?
# TODO: niezależne katalogi linux-gcc linux-clang ?
# TODO: uzależnić *_merged.cpp od listy plików a nie całego makefile-a
# TODO: wspólna konfiguracja dla Makefile, Makefile.include i makefile-i dla aplikacji

PLATFORM      = linux
MODE          = release
MINGW_PREFIX ?= x86_64-w64-mingw32.static.posix-
LINKER        = $(COMPILER)
BUILD_DIR     = build/$(PLATFORM)_$(MODE)
LIB_FILE      = lib/libfwk_$(PLATFORM)_$(MODE).a
GEOM          = enabled
IMGUI        ?= $(if $(wildcard extern/imgui/imgui.h),enabled,disabled)

# Checking platform:
ifeq ($(PLATFORM), linux)
COMPILER       = clang++
else ifeq ($(PLATFORM), mingw)
COMPILER       = $(MINGW_PREFIX)g++
TOOLSET_PREFIX = $(MINGW_PREFIX)
PROGRAM_SUFFIX =.exe
else ifeq ($(PLATFORM), html)
COMPILER       = emcc
PROGRAM_SUFFIX =.html
else
$(error ERROR: Unrecognized platform: $(PLATFORM))
endif

# --- Creating necessary sub-directories ----------------------------------------------------------

SUBDIRS        = build tests tools lib temp
BUILD_SUBDIRS  = tests tools
ifdef SPLIT_MODULES
BUILD_SUBDIRS += gfx audio math sys tests tools menu perf geom
endif

_dummy := $(shell mkdir -p $(SUBDIRS))
_dummy := $(shell mkdir -p $(addprefix $(BUILD_DIR)/,$(BUILD_SUBDIRS)))

# --- Lists of source files -----------------------------------------------------------------------

SRC_base = base_vector enum str type_info any any_ref logger any_config format parse

SRC_sys  = \
	sys_base sys/file_system sys/file_system_linux sys/file_stream sys/error sys/exception \
	sys/expected sys/assert sys/assert_impl sys/on_fail sys/memory sys/backtrace sys/xml sys/input

SRC_math = \
	math/cylinder math/box math/obox math/frustum math/matrix3 math/matrix4 math/plane math/ray math/rotation \
	math/quat math/base math/triangle math/tetrahedron math/projection math/random math/segment math/line \
	math/affine_trans math/rational math/gcd math/rational_angle

SRC_gfx = \
	gfx/camera gfx/fps_camera gfx/ortho_camera gfx/orbiting_camera gfx/plane_camera gfx/camera_control \
	gfx/color gfx/font gfx/font_factory gfx/colored_triangle gfx/colored_quad gfx/material \
	gfx/element_buffer gfx/triangle_buffer gfx/line_buffer gfx/sprite_buffer \
	gfx/texture gfx/texture_tga gfx/texture_png gfx/texture_bmp \
	gfx/material_set gfx/matrix_stack gfx/draw_call \
	gfx/visualizer2 gfx/visualizer3

SRC_gfx_mesh = \
	gfx/model_anim gfx/model_node gfx/dynamic_mesh gfx/model gfx/pose gfx/mesh gfx/mesh_indices \
	gfx/mesh_buffers gfx/mesh_constructor gfx/animated_model gfx/converter

SRC_gfx_gl = \
	gfx/gl_device gfx/gl_texture gfx/gl_renderbuffer gfx/gl_framebuffer gfx/gl_shader gfx/gl_storage \
	gfx/gl_format gfx/gl_query  gfx/gl_buffer gfx/gl_vertex_array gfx/gl_program \
	gfx/render_list gfx/renderer2d gfx/opengl

SRC_audio = audio/al_device audio/sound audio/ogg_stream

ifeq ($(GEOM), enabled)
SRC_geom = geom_base geom/contour geom/regular_grid geom/segment_grid geom/procgen
SRC_geom_graph = geom/element_ref geom/graph geom/geom_graph
SRC_geom_voronoi = geom/voronoi geom/wide_int geom/voronoi_constructor geom/delaunay
endif

ifeq ($(IMGUI), enabled)
SRC_menu_imgui = menu/imgui_code
SRC_menu = menu/open_file_popup menu/error_popup menu/helpers menu/imgui_wrapper perf/analyzer
SRC_perf = perf/perf_base perf/exec_tree perf/manager perf/thread_context
endif

SRC_tests = \
	tests/stuff tests/math tests/geom tests/window tests/enums tests/models tests/vector_perf \
	tests/variant_perf

SRC_tools    = tools/model_convert tools/model_viewer
SRC_programs = $(SRC_tests) $(SRC_tools)

MODULES = menu_imgui base sys gfx gfx_gl gfx_mesh math geom geom_graph geom_voronoi menu perf audio

# --- Definitions ---------------------------------------------------------------------------------

SRC_merged = $(MODULES:%=%_merged)
SRC_shared = $(foreach MODULE,$(MODULES),$(SRC_$(MODULE)))
SRC_all    = $(SRC_merged) $(SRC_shared) $(SRC_programs)

CPP_merged = $(SRC_merged:%=$(BUILD_DIR)/%.cpp)
CPP_shared = $(SRC_shared:%=src/%.cpp)

SHARED_OBJECTS := $(SRC_shared:%=$(BUILD_DIR)/%.o)
MERGED_OBJECTS := $(SRC_merged:%=$(BUILD_DIR)/%.o)
OBJECTS        := $(SHARED_OBJECTS) $(SRC_programs:%=$(BUILD_DIR)/%.o)

INPUT_OBJECTS  := $(if $(SPLIT_MODULES),$(SHARED_OBJECTS),$(MERGED_OBJECTS))
INPUT_SRCS     := $(if $(SPLIT_MODULES),$(CPP_shared),$(CPP_merged))

PROGRAMS       := $(SRC_programs:%=%$(PROGRAM_SUFFIX))
#HTML5_PROGRAMS_SRC:=$(SRC_programs:%=%.html.cpp)

ARCHIVER       = $(TOOLSET_PREFIX)ar
STRIPPER       = $(TOOLSET_PREFIX)strip
PKG_CONFIG     = $(TOOLSET_PREFIX)pkg-config

COMPILER_TYPE = $(if $(findstring clang,$(COMPILER)),clang,$(if $(findstring mingw,$(PLATFORM)),gcc,clang))

ifeq ($(COMPILER_TYPE),clang)
LINKER += -fuse-ld=gold
endif

STATS_FILE = $(BUILD_DIR)/stats.txt
ifdef STATS
STATS_CMD  = time -o $(STATS_FILE) -a -f "%U $*.o"
endif

all:   $(LIB_FILE) $(PROGRAMS)
tools: $(SRC_tools:%=%$(PROGRAM_SUFFIX))
tests: $(SRC_tests:%=%$(PROGRAM_SUFFIX))

# --- Compilation & linking flags -----------------------------------------------------------------

INCLUDES = -Iinclude/ -Isrc/

LIBS_shared = freetype2 sdl2 libpng vorbisfile
LIBS_linux  = $(shell $(PKG_CONFIG) --libs $(LIBS_shared)) -lopenal -lGL -lGLU -lrt -lm -lstdc++
LIBS_mingw  = $(shell $(PKG_CONFIG) --libs $(LIBS_shared)) -lOpenAL32 -ldsound -lole32 -lwinmm \
			  -lglu32 -lopengl32 -lws2_32 -limagehlp

# Clang gives no warnings for uninitialized class members!
#TODO:-Wno-strict-overflow
FLAGS_shared = -fno-exceptions -std=c++2a -DFATAL=FWK_FATAL -DDUMP=FWK_DUMP \
			   -Wall -Wextra -Woverloaded-virtual -Wnon-virtual-dtor -Wno-reorder -Wuninitialized \
			   -Wno-unused-function -Werror=switch -Wno-unused-variable -Wno-unused-parameter \
			   -Wparentheses -Wno-overloaded-virtual
FLAGS_clang  = -Wconstant-conversion -Werror=return-type -Wno-undefined-inline
FLAGS_gcc    = -Werror=aggressive-loop-optimizations -Wno-unused-but-set-variable

FLAGS_release = -O3
FLAGS_debug   = -O0 -DFWK_PARANOID

FLAGS_linux  = $(shell $(PKG_CONFIG) --cflags $(LIBS_shared)) -pthread -ggdb -Umain -DFWK_TARGET_LINUX 
FLAGS_mingw  = $(shell $(PKG_CONFIG) --cflags $(LIBS_shared)) -pthread -ggdb -Umain -DFWK_TARGET_MINGW -msse2 -mfpmath=sse

FLAGS       += $(FLAGS_$(MODE)) $(INCLUDES) $(FLAGS_$(PLATFORM)) $(FLAGS_shared) $(FLAGS_$(COMPILER_TYPE)) \
			   $(if $(findstring enabled,$(GEOM)),,-DFWK_GEOM_DISABLED) \
			   $(if $(findstring enabled,$(IMGUI)),,-DFWK_IMGUI_DISABLED) \
			   $(ADD_FLAGS)

LINK_FLAGS_clang = -fuse-ld=gold
LINK_FLAGS_linux = -rdynamic
LINK_FLAGS       = -pthread  $(LIBS_$(PLATFORM)) $(LINK_FLAGS_$(PLATFORM)) $(LINK_FLAGS_$(COMPILER_TYPE))

#HTML5_NICE_FLAGS=-s ASSERTIONS=2 -s DISABLE_EXCEPTION_CATCHING=0 -g2
#HTML5_FLAGS=-DFWK_TARGET_HTML5 -DNDEBUG --memory-init-file 0 -s USE_SDL=2 -s USE_LIBPNG=1 -s USE_VORBIS=1 \
			--embed-file data/ $(INCLUDES) $(FLAGS) $(CLANG_FLAGS)

# --- Precompiled headers configuration -----------------------------------------------------------

# TODO: check if it's even enabled
PCH_FILE_SRC = src/fwk_pch.h

PCH_FILE_H    = $(BUILD_DIR)/fwk_pch_.h
PCH_FILE_GCH  = $(BUILD_DIR)/fwk_pch_.h.gch
PCH_FILE_PCH  = $(BUILD_DIR)/fwk_pch_.h.pch

ifeq ($(COMPILER_TYPE),clang)
PCH_INCLUDE   = -include-pch $(PCH_FILE_PCH)
PCH_FILE_MAIN = $(PCH_FILE_PCH)
else
PCH_INCLUDE   = -I$(BUILD_DIR) -include $(PCH_FILE_H)
PCH_FILE_MAIN = $(PCH_FILE_GCH)
endif

$(PCH_FILE_H): $(PCH_FILE_SRC)
	cp $^ $@
$(PCH_FILE_MAIN): $(PCH_FILE_H)
	$(COMPILER) -x c++-header -MMD $(FLAGS) $(PCH_FILE_H) -o $@

# --- Exception checker ---------------------------------------------------------------------------

ifeq ($(COMPILER_TYPE),clang)
checker.so: .always_check
	$(MAKE) -C src/checker/ ../../checker.so

ifneq ("$(wildcard checker.so)","")
FLAGS += -Xclang -load -Xclang  $(realpath checker.so) -Xclang -add-plugin -Xclang check-fwk-exceptions
endif
endif

# --- Main build targets --------------------------------------------------------------------------

$(CPP_merged): $(BUILD_DIR)/%_merged.cpp: Makefile
	@echo "$(SRC_$*:%=#include \"%.cpp\"\n)" > $@

$(OBJECTS): $(BUILD_DIR)/%.o: src/%.cpp $(PCH_FILE_MAIN)
	$(STATS_CMD) $(COMPILER) -MMD $(FLAGS) $(PCH_INCLUDE) -c src/$*.cpp -o $@

$(MERGED_OBJECTS): $(BUILD_DIR)/%.o: $(BUILD_DIR)/%.cpp $(PCH_FILE_MAIN)
	$(STATS_CMD) $(COMPILER) -MMD $(FLAGS) $(PCH_INCLUDE) -c $(BUILD_DIR)/$*.cpp -o $@

$(PROGRAMS): %$(PROGRAM_SUFFIX): $(INPUT_OBJECTS) $(BUILD_DIR)/%.o
	$(LINKER) -MMD -o $@ $^ $(LINK_FLAGS)

#$(HTML5_PROGRAMS_SRC): %.html.cpp: src/%.cpp $(SRC_shared:%=src/%.cpp)
#	cat src/html_pre_include.cpp $^ > $@
#$(HTML5_PROGRAMS): %.html: %.html.cpp
#	emcc $(HTML5_FLAGS) $^ -o $@

# TODO: use Makefile.include to build test programs?

$(LIB_FILE): $(INPUT_OBJECTS)
	$(ARCHIVER) r $@ $^ 

#lib/libfwk.cpp: $(SRC_shared:%=src/%.cpp)
# TODO: catting sux
#	cat $^ > $@
#lib/libfwk.html.cpp: $(SRC_shared:%=src/%.cpp) $(HTML5_SRC:%=src/%.cpp)
#	cat $^ > $@

DEPS:=$(SRC_all:%=$(BUILD_DIR)/%.d) $(PCH_FILE_H).d

print-stats:
	sort -n -r $(STATS_FILE)

clean:
	-rm -f $(OBJECTS) $(DEPS) $(MERGED_OBJECTS) $(PROGRAMS) $(CPP_merged) $(STATS_FILE) \
		$(LIB_FILE) $(PCH_FILE_GCH) $(PCH_FILE_PCH) $(PCH_FILE_H)
	find $(SUBDIRS) -type d -empty -delete

clean-checker:
	$(MAKE) -C src/checker/ clean

clean-all: clean-checker
	$(MAKE) -C . PLATFORM=linux MODE=debug   clean
	$(MAKE) -C . PLATFORM=linux MODE=release clean
	$(MAKE) -C . PLATFORM=mingw MODE=debug   clean
	$(MAKE) -C . PLATFORM=mingw MODE=release clean
	
print-variables:
	@echo PLATFORM=$(PLATFORM)
	@echo MODE=$(MODE)
	@echo COMPILER=$(COMPILER)
	@echo COMPILER_TYPE=$(COMPILER_TYPE)
	@echo LINKER=$(LINKER)
	@echo LIB_FILE=$(LIB_FILE)
	@echo 
	@echo IMGUI=$(IMGUI)
	@echo GEOM=$(GEOM)
	@echo 
	@echo OBJECTS=$(OBJECTS)
	@echo MERGED_OBJECTS=$(MERGED_OBJECTS)
	@echo 
	@echo FLAGS=$(FLAGS)
	@echo LINK_FLAGS=$(LINK_FLAGS)

.PHONY: clean tools tests clean-all clean-checker
.always_check:

-include $(DEPS)
