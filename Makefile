MINGW_PREFIX=x86_64-w64-mingw32.static.posix-
BUILD_DIR=build
LINUX_CXX=clang++
FLAGS=-O3

-include Makefile.local

ifneq (,$(findstring clang,$(LINUX_CXX)))
CLANG=yes
endif

_dummy := $(shell [ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR))
_dummy := $(shell [ -d $(BUILD_DIR)/gfx ] || mkdir -p $(BUILD_DIR)/gfx)
_dummy := $(shell [ -d $(BUILD_DIR)/audio ] || mkdir -p $(BUILD_DIR)/audio)
_dummy := $(shell [ -d $(BUILD_DIR)/math ] || mkdir -p $(BUILD_DIR)/math)
_dummy := $(shell [ -d $(BUILD_DIR)/sys ] || mkdir -p $(BUILD_DIR)/sys)
_dummy := $(shell [ -d $(BUILD_DIR)/tests ] || mkdir -p $(BUILD_DIR)/tests)
_dummy := $(shell [ -d $(BUILD_DIR)/tools ] || mkdir -p $(BUILD_DIR)/tools)
_dummy := $(shell [ -d tests ] || mkdir -p tests)
_dummy := $(shell [ -d tools ] || mkdir -p tools)
_dummy := $(shell [ -d lib ] || mkdir -p lib)
_dummy := $(shell [ -d temp ] || mkdir -p temp)


SHARED_SRC=vector filesystem filesystem_linux profiler enum str sys_base type_info any any_ref \
		   sys/error sys/assert sys/on_fail sys/memory sys/backtrace sys/rollback sys/stream sys/xml sys/input \
           math/cylinder math/box math/frustum math/matrix3 math/matrix4 math/plane math/ray math/vector \
		   math/quat math/base math/triangle math/tetrahedron math/projection math/random math/segment math/interval \
		   math/affine_trans \
		   format parse audio/device audio/sound audio/ogg_stream \
		   gfx/color gfx/gfx_device gfx/device_texture gfx/font gfx/font_factory gfx/model_anim gfx/model_node \
		   gfx/material gfx/material_set gfx/matrix_stack gfx/opengl gfx/model gfx/draw_call gfx/program_binder gfx/pose \
		   gfx/mesh gfx/mesh_indices gfx/mesh_buffers gfx/mesh_constructor gfx/animated_model gfx/converter \
		   gfx/vertex_array gfx/vertex_buffer gfx/index_buffer gfx/render_buffer gfx/frame_buffer gfx/shader \
		   gfx/program gfx/render_list gfx/renderer2d gfx/dynamic_mesh gfx/shader_storage gfx/colored_triangle \
		   gfx/texture gfx/texture_format gfx/texture_tga gfx/texture_png gfx/texture_bmp gfx/opengl_query \
		   gfx/element_buffer gfx/triangle_buffer gfx/line_buffer gfx/sprite_buffer

TESTS_SRC=tests/streams tests/stuff tests/math tests/window tests/enums tests/models tests/vector tests/vector_perf \
			tests/variant_perf tests/rollback_test
TOOLS_SRC=tools/model_convert tools/model_viewer
PROGRAM_SRC=$(TESTS_SRC) $(TOOLS_SRC)


ALL_SRC=$(SHARED_SRC) $(PROGRAM_SRC)
WINDOWS_SRC=system_windows filesystem_windows

LINUX_SHARED_OBJECTS:=$(SHARED_SRC:%=$(BUILD_DIR)/%.o)
MINGW_SHARED_OBJECTS:=$(SHARED_SRC:%=$(BUILD_DIR)/%_.o) $(WINDOWS_SRC:%=$(BUILD_DIR)/%_.o)

LINUX_OBJECTS:=$(LINUX_SHARED_OBJECTS) $(PROGRAM_SRC:%=$(BUILD_DIR)/%.o)
MINGW_OBJECTS:=$(MINGW_SHARED_OBJECTS) $(PROGRAM_SRC:%=$(BUILD_DIR)/%_.o)

LINUX_PROGRAMS:=$(PROGRAM_SRC:%=%)
MINGW_PROGRAMS:=$(PROGRAM_SRC:%=%.exe)
HTML5_PROGRAMS:=$(PROGRAM_SRC:%=%.html)
HTML5_PROGRAMS_SRC:=$(PROGRAM_SRC:%=%.html.cpp)

all: lib/libfwk.a lib/libfwk_win32.a lib/libfwk.cpp $(LINUX_PROGRAMS) $(MINGW_PROGRAMS)
tools: $(TOOLS_SRC)
tests: $(TESTS_SRC)
tools_mingw: $(TOOLS_SRC:%=%.exe)
tests_mingw: $(TESTS_SRC:%=%.exe)

LINUX_AR =ar
LINUX_STRIP=strip
LINUX_PKG_CONFIG=pkg-config

MINGW_CXX=$(MINGW_PREFIX)g++
MINGW_STRIP=$(MINGW_PREFIX)strip
MINGW_AR=$(MINGW_PREFIX)ar
MINGW_PKG_CONFIG=$(MINGW_PREFIX)pkg-config

LIBS=freetype2 sdl2 libpng vorbisfile
LINUX_LIBS=-pthread $(shell $(LINUX_PKG_CONFIG) --libs $(LIBS)) -lgmp -lmpfr -lopenal -lGL -lGLU -lrt -lm -lstdc++
MINGW_LIBS=-pthread $(shell $(MINGW_PKG_CONFIG) --libs $(LIBS)) -lOpenAL32 -ldsound -lole32 -lwinmm -lglu32 -lopengl32\
		   -lws2_32 -limagehlp

INCLUDES=-Iinclude/ -Isrc/

# Clang gives no warnings for uninitialized class members!
NICE_FLAGS=-std=c++1z -fno-exceptions -Wall -Wextra -Woverloaded-virtual -Wnon-virtual-dtor -Werror=return-type -Wno-reorder \
		   -Wuninitialized -Wno-unused-function -Werror=switch -Wno-unused-variable -Wno-unused-parameter \
		   -Wparentheses -Wno-overloaded-virtual -Wno-undefined-inline #-Werror
HTML5_NICE_FLAGS=-s ASSERTIONS=2 -s DISABLE_EXCEPTION_CATCHING=0 -g2
LINUX_FLAGS=-DFWK_TARGET_LINUX -pthread -ggdb $(shell $(LINUX_PKG_CONFIG) --cflags $(LIBS)) -Umain $(NICE_FLAGS) \
			$(INCLUDES) $(FLAGS)
MINGW_FLAGS=-DFWK_TARGET_MINGW -pthread -ggdb -msse2 -mfpmath=sse $(shell $(MINGW_PKG_CONFIG) --cflags $(LIBS)) -Umain \
			$(NICE_FLAGS) $(INCLUDES) $(FLAGS)
HTML5_FLAGS=-DFWK_TARGET_HTML5 -DNDEBUG --memory-init-file 0 -O2 -s USE_SDL=2 -s USE_LIBPNG=1 -s USE_VORBIS=1 \
			--embed-file data/ $(NICE_FLAGS) $(INCLUDES)

PCH_FILE_SRC=src/pch.h

PCH_FILE_H=$(BUILD_DIR)/pch.h
PCH_FILE_GCH=$(BUILD_DIR)/pch.h.gch
PCH_FILE_PCH=$(BUILD_DIR)/pch.h.pch

ifdef CLANG
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

$(LINUX_OBJECTS): $(BUILD_DIR)/%.o: src/%.cpp $(PCH_FILE_MAIN)
	$(LINUX_CXX) -MMD $(LINUX_FLAGS) $(PCH_INCLUDE) -c src/$*.cpp -o $@

$(MINGW_OBJECTS): $(BUILD_DIR)/%_.o: src/%.cpp
	$(MINGW_CXX) -MMD $(MINGW_FLAGS) -c src/$*.cpp -o $@

$(LINUX_PROGRAMS): %:     $(LINUX_SHARED_OBJECTS) $(BUILD_DIR)/%.o
	$(LINUX_CXX) -MMD -o $@ $^ -rdynamic $(LINUX_LIBS) $(LIBS_$@)

$(MINGW_PROGRAMS): %.exe: $(MINGW_SHARED_OBJECTS) $(BUILD_DIR)/%_.o
	$(MINGW_CXX) -MMD -o $@ $^ $(MINGW_LIBS) $(LIBS_$*)
#	$(MINGW_STRIP) $@

$(HTML5_PROGRAMS_SRC): %.html.cpp: src/%.cpp $(SHARED_SRC:%=src/%.cpp)
	cat src/html_pre_include.cpp $^ > $@

$(HTML5_PROGRAMS): %.html: %.html.cpp
	emcc $(HTML5_FLAGS) $^ -o $@

# TODO: use Makefile.include to build test programs

lib/libfwk.a: $(LINUX_SHARED_OBJECTS)
	$(LINUX_AR) r $@ $^ 

lib/libfwk_win32.a: $(MINGW_SHARED_OBJECTS)
	$(MINGW_AR) r $@ $^

lib/libfwk.cpp: $(SHARED_SRC:%=src/%.cpp)
	cat $^ > $@

lib/libfwk.html.cpp: $(SHARED_SRC:%=src/%.cpp) $(HTML5_SRC:%=src/%.cpp)
	cat $^ > $@

DEPS:=$(ALL_SRC:%=$(BUILD_DIR)/%.d) $(ALL_SRC:%=$(BUILD_DIR)/%_.d) $(PCH_FILE_H).d

clean:
	-rm -f $(LINUX_OBJECTS) $(MINGW_OBJECTS) $(LINUX_PROGRAMS) $(MINGW_PROGRAMS) \
		$(HTML5_PROGRAMS) $(HTML5_PROGRAMS_SRC) $(HTML5_PROGRAMS:%.html=%.js) \
		$(DEPS) lib/libfwk.a lib/libfwk_win32.a lib/libfwk.cpp lib/libfwk.html.cpp \
		$(PCH_FILE_GCH) $(PCH_FILE_PCH) $(PCH_FILE_H)
	-rmdir tests temp lib tools
	find $(BUILD_DIR) -type d -empty -delete

.PHONY: clean tools

-include $(DEPS)
