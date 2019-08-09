# Options:
# BUILD_STATS: gathers build times for each object file
# FAST_BUILD: builds modules by merging multiple cpp files

MINGW_PREFIX=x86_64-w64-mingw32.static.posix-
BUILD_DIR=build
LINUX_CXX=clang++
FLAGS=-O3

-include Makefile.local

ifndef LINUX_LINK
	LINUX_LINK=$(LINUX_CXX)
endif

ifneq (,$(findstring clang,$(LINUX_CXX)))
	LINUX_CLANG_MODE=yes
	LINUX_LINK+=-fuse-ld=gold
endif

ifneq ("$(wildcard extern/imgui/imgui.h)","")
	IMGUI_ENABLED=true
	FLAGS+=-DFWK_IMGUI_ENABLED
endif

ifdef BUILD_STATS
	STATS_CMD=time -o build_stats.txt -a -f "%U $@"
endif

# --- Creating necessary sub-directories ----------------------------------------------------------

ifndef FAST_BUILD
BUILD_SUBDIRS = gfx audio math sys tests tools menu perf geom
endif
BUILD_SUBDIRS+= tests tools
SUBDIRS       = build tests tools lib temp

_dummy := $(shell mkdir -p $(SUBDIRS))
_dummy := $(shell mkdir -p $(addprefix $(BUILD_DIR)/,$(BUILD_SUBDIRS)))

# --- Lists of source files -----------------------------------------------------------------------

SRC_base = vector enum str type_info any any_ref logger any_config format parse

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

SRC_geom = geom/contour geom/regular_grid geom/segment_grid geom/procgen
SRC_geom_graph= geom/immutable_graph geom/plane_graph geom/plane_graph_builder  geom/graph geom/geom_graph
SRC_geom_voronoi = geom/voronoi geom/wide_int geom/voronoi_constructor geom/delaunay

ifdef IMGUI_ENABLED
SRC_menu_imgui = menu/imgui_code
SRC_menu = menu/open_file_popup menu/error_popup menu/helpers menu/imgui_wrapper perf/analyzer
SRC_perf = perf/perf_base perf/exec_tree perf/manager perf/thread_context
endif

SRC_tests = \
	tests/stuff tests/math tests/geom tests/window tests/enums tests/models tests/vector_perf \
	tests/variant_perf

SRC_tools = tools/model_convert tools/model_viewer
SRC_programs = $(SRC_tests) $(SRC_tools)

MODULES=menu_imgui base sys gfx gfx_gl gfx_mesh math geom geom_graph geom_voronoi menu perf audio

# --- Definitions ---------------------------------------------------------------------------------

SRC_merged = $(MODULES:%=%_merged)
SRC_shared = $(SRC_menu_imgui) $(SRC_base) $(SRC_sys) $(SRC_math) $(SRC_gfx) $(SRC_gfx_gl) \
			 $(SRC_gfx_mesh) $(SRC_geom) $(SRC_geom_graph) $(SRC_geom_voronoi) $(SRC_menu) \
			 $(SRC_perf) $(SRC_audio)

SRC_all=$(SRC_merged) $(SRC_shared) $(SRC_programs)

CPP_merged = $(SRC_merged:%=$(BUILD_DIR)/%.cpp)
CPP_shared = $(SRC_shared:%=src/%.cpp)

LINUX_SHARED_OBJECTS:=$(SRC_shared:%=$(BUILD_DIR)/%.o)
MINGW_SHARED_OBJECTS:=$(SRC_shared:%=$(BUILD_DIR)/%_.o)

LINUX_MERGED_OBJECTS:=$(SRC_merged:%=$(BUILD_DIR)/%.o)
MINGW_MERGED_OBJECTS:=$(SRC_merged:%=$(BUILD_DIR)/%_.o)

LINUX_OBJECTS:=$(LINUX_SHARED_OBJECTS) $(SRC_programs:%=$(BUILD_DIR)/%.o)
MINGW_OBJECTS:=$(MINGW_SHARED_OBJECTS) $(SRC_programs:%=$(BUILD_DIR)/%_.o)

ifdef FAST_BUILD
	LINUX_INPUT_OBJECTS = $(LINUX_MERGED_OBJECTS)
	MINGW_INPUT_OBJECTS = $(MINGW_MERGED_OBJECTS)
	INPUT_SRCS = $(CPP_merged)
else
	LINUX_INPUT_OBJECTS = $(LINUX_SHARED_OBJECTS)
	MINGW_INPUT_OBJECTS = $(MINGW_SHARED_OBJECTS)
	INPUT_SRCS = $(CPP_shared)
endif

LINUX_PROGRAMS:=$(SRC_programs:%=%)
MINGW_PROGRAMS:=$(SRC_programs:%=%.exe)
HTML5_PROGRAMS:=$(SRC_programs:%=%.html)
HTML5_PROGRAMS_SRC:=$(SRC_programs:%=%.html.cpp)

LINUX_AR =ar
LINUX_STRIP=strip
LINUX_PKG_CONFIG=pkg-config

MINGW_CXX=$(MINGW_PREFIX)g++
MINGW_STRIP=$(MINGW_PREFIX)strip
MINGW_AR=$(MINGW_PREFIX)ar
MINGW_PKG_CONFIG=$(MINGW_PREFIX)pkg-config

# --- Compilation & linking flags -----------------------------------------------------------------

LIBS=freetype2 sdl2 libpng vorbisfile
LINUX_LIBS=-pthread $(shell $(LINUX_PKG_CONFIG) --libs $(LIBS)) -lopenal -lGL -lGLU -lrt -lm -lstdc++
MINGW_LIBS=-pthread $(shell $(MINGW_PKG_CONFIG) --libs $(LIBS)) -lOpenAL32 -ldsound -lole32 -lwinmm \
		   -lglu32 -lopengl32 -lws2_32 -limagehlp

INCLUDES=-Iinclude/ -Isrc/


CLANG_FLAGS+=-Wconstant-conversion -Werror=return-type -Wno-undefined-inline
GCC_FLAGS+=-Werror=aggressive-loop-optimizations -Wno-unused-but-set-variable

# Clang gives no warnings for uninitialized class members!
FLAGS+=-Wall -Wextra -Woverloaded-virtual -Wnon-virtual-dtor -Wno-reorder -Wuninitialized -Wno-unused-function \
		-Werror=switch -Wno-unused-variable -Wno-unused-parameter -Wparentheses -Wno-overloaded-virtual
#TODO:-Wno-strict-overflow

FLAGS+=-fno-exceptions -std=c++2a -DFATAL=FWK_FATAL -DDUMP=FWK_DUMP
LINUX_FLAGS=-DFWK_TARGET_LINUX -pthread -ggdb $(shell $(LINUX_PKG_CONFIG) --cflags $(LIBS)) -Umain \
			$(INCLUDES) $(FLAGS)
MINGW_FLAGS=-DFWK_TARGET_MINGW -pthread -ggdb -msse2 -mfpmath=sse $(shell $(MINGW_PKG_CONFIG) --cflags $(LIBS)) \
			-Umain $(INCLUDES) $(FLAGS) $(GCC_FLAGS)

HTML5_NICE_FLAGS=-s ASSERTIONS=2 -s DISABLE_EXCEPTION_CATCHING=0 -g2
HTML5_FLAGS=-DFWK_TARGET_HTML5 -DNDEBUG --memory-init-file 0 -s USE_SDL=2 -s USE_LIBPNG=1 -s USE_VORBIS=1 \
			--embed-file data/ $(INCLUDES) $(FLAGS) $(CLANG_FLAGS)

ifdef LINUX_CLANG_MODE
	LINUX_FLAGS += $(CLANG_FLAGS)
else
	LINUX_FLAGS += $(GCC_FLAGS)
endif

# --- Precompiled headers configuration -----------------------------------------------------------

PCH_FILE_SRC=src/fwk_pch.h

PCH_FILE_H=$(BUILD_DIR)/fwk_pch_.h
PCH_FILE_GCH=$(BUILD_DIR)/fwk_pch_.h.gch
PCH_FILE_PCH=$(BUILD_DIR)/fwk_pch_.h.pch

ifdef LINUX_CLANG_MODE
	PCH_INCLUDE=-include-pch $(PCH_FILE_PCH)
	PCH_FILE_MAIN=$(PCH_FILE_PCH)
else
	PCH_INCLUDE=-I$(BUILD_DIR) -include $(PCH_FILE_H)
	PCH_FILE_MAIN=$(PCH_FILE_GCH)
endif

$(PCH_FILE_H): $(PCH_FILE_SRC)
	cp $^ $@
$(PCH_FILE_MAIN): $(PCH_FILE_H)
	$(LINUX_CXX) -x c++-header -MMD $(LINUX_FLAGS) $(PCH_FILE_H) -o $@

# --- Exception checker ---------------------------------------------------------------------------

ifdef LINUX_CLANG_MODE
checker.so: .ALWAYS_CHECK
	$(MAKE) -C src/checker/ ../../checker.so

ifneq ("$(wildcard checker.so)","")
LINUX_CHECKER_FLAGS=-Xclang -load -Xclang  $(realpath checker.so) -Xclang -add-plugin -Xclang check-fwk-exceptions
endif
endif

# --- Main build targets --------------------------------------------------------------------------

all: lib/libfwk.a lib/libfwk_win32.a lib/libfwk.cpp $(LINUX_PROGRAMS) $(MINGW_PROGRAMS)
tools: $(SRC_tools)
tests: $(SRC_tests)
tools_mingw: $(SRC_tools:%=%.exe)
tests_mingw: $(SRC_tests:%=%.exe)

$(CPP_merged): $(BUILD_DIR)/%_merged.cpp: Makefile
	@echo "$(SRC_$*:%=#include \"%.cpp\"\n)" > $@

$(LINUX_OBJECTS): $(BUILD_DIR)/%.o: src/%.cpp $(PCH_FILE_MAIN)
	$(STATS_CMD) $(LINUX_CXX) -MMD $(LINUX_FLAGS) $(LINUX_CHECKER_FLAGS) $(PCH_INCLUDE) -c src/$*.cpp -o $@

$(LINUX_MERGED_OBJECTS): $(BUILD_DIR)/%.o: $(BUILD_DIR)/%.cpp $(PCH_FILE_MAIN)
	$(STATS_CMD) $(LINUX_CXX) -MMD $(LINUX_FLAGS) $(LINUX_CHECKER_FLAGS) $(PCH_INCLUDE) -c $(BUILD_DIR)/$*.cpp -o $@

$(MINGW_OBJECTS): $(BUILD_DIR)/%_.o: src/%.cpp
	$(MINGW_CXX) -MMD $(MINGW_FLAGS) -c src/$*.cpp -o $@

$(MINGW_MERGED_OBJECTS): $(BUILD_DIR)/%_.o: $(BUILD_DIR)/%.cpp
	$(MINGW_CXX) -MMD $(MINGW_FLAGS) -c $(BUILD_DIR)/$*.cpp -o $@

$(LINUX_PROGRAMS): %:     $(LINUX_INPUT_OBJECTS) $(BUILD_DIR)/%.o
	$(LINUX_LINK) -MMD -o $@ $^ -rdynamic $(LINUX_LIBS) $(LIBS_$@)

$(MINGW_PROGRAMS): %.exe: $(MINGW_INPUT_OBJECTS) $(BUILD_DIR)/%_.o
	$(MINGW_CXX) -MMD -o $@ $^ $(MINGW_LIBS) $(LIBS_$*)
#	$(MINGW_STRIP) $@

$(HTML5_PROGRAMS_SRC): %.html.cpp: src/%.cpp $(SRC_shared:%=src/%.cpp)
	cat src/html_pre_include.cpp $^ > $@

$(HTML5_PROGRAMS): %.html: %.html.cpp
	emcc $(HTML5_FLAGS) $^ -o $@

# TODO: use Makefile.include to build test programs

lib/libfwk.a: $(LINUX_INPUT_OBJECTS)
	$(LINUX_AR) r $@ $^ 

lib/libfwk_win32.a: $(MINGW_INPUT_OBJECTS)
	$(MINGW_AR) r $@ $^

lib/libfwk.cpp: $(SRC_shared:%=src/%.cpp)
	cat $^ > $@

lib/libfwk.html.cpp: $(SRC_shared:%=src/%.cpp) $(HTML5_SRC:%=src/%.cpp)
	cat $^ > $@

DEPS:=$(SRC_all:%=$(BUILD_DIR)/%.d) $(SRC_all:%=$(BUILD_DIR)/%_.d) $(PCH_FILE_H).d

show-stats:
	@sort -n -r build_stats.txt

clean:
	-rm -f $(LINUX_OBJECTS) $(MINGW_OBJECTS) $(LINUX_MERGED_OBJECTS) $(MINGW_MERGED_OBJECTS) \
		$(LINUX_PROGRAMS) $(MINGW_PROGRAMS) $(HTML5_PROGRAMS) $(HTML5_PROGRAMS_SRC) \
		$(CPP_merged) $(HTML5_PROGRAMS:%.html=%.js) build_stats.txt \
		$(DEPS) lib/libfwk.a lib/libfwk_win32.a lib/libfwk.cpp lib/libfwk.html.cpp \
		$(PCH_FILE_GCH) $(PCH_FILE_PCH) $(PCH_FILE_H)
	find $(SUBDIRS) -type d -empty -delete
	
checker-clean:
	$(MAKE) -C src/checker/ clean

full-clean: clean checker-clean

.PHONY: clean tools
.ALWAYS_CHECK:

-include $(DEPS)
