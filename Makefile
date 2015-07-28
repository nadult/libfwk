MINGW_PREFIX=i686-w64-mingw32.static-
BUILD_DIR=build
LINUX_CXX=g++ -rdynamic

-include Makefile.local

_dummy := $(shell [ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR))
_dummy := $(shell [ -d $(BUILD_DIR)/gfx ] || mkdir -p $(BUILD_DIR)/gfx)
_dummy := $(shell [ -d $(BUILD_DIR)/math ] || mkdir -p $(BUILD_DIR)/math)
_dummy := $(shell [ -d $(BUILD_DIR)/test ] || mkdir -p $(BUILD_DIR)/test)
_dummy := $(shell [ -d $(BUILD_DIR)/tools ] || mkdir -p $(BUILD_DIR)/tools)
_dummy := $(shell [ -d test ] || mkdir -p test)
_dummy := $(shell [ -d tools ] || mkdir -p tools)
_dummy := $(shell [ -d lib ] || mkdir -p lib)
_dummy := $(shell [ -d temp ] || mkdir -p temp)


SHARED_SRC=base backtrace filesystem filesystem_linux filesystem_windows input profiler stream xml xml_conversions cache \
		   gfx/color gfx/device gfx/device_texture gfx/font gfx/font_factory gfx/model_anim gfx/model_tree gfx/material gfx/matrix_stack \
		   gfx/opengl gfx/texture gfx/texture_format gfx/texture_tga gfx/model gfx/mesh gfx/mesh_indices gfx/mesh_buffers gfx/mesh_constructor \
		   gfx/vertex_array gfx/vertex_buffer gfx/index_buffer gfx/shader gfx/program gfx/renderer gfx/renderer2d math/cylinder \
		   math/box math/frustum math/matrix3 math/matrix4 math/plane math/ray math/rect math/vector math/quat math/base math/triangle \
		   text_formatter text_parser
PROGRAM_SRC=test/streams test/stuff test/math test/window test/enums test/models tools/model_convert tools/model_viewer


ALL_SRC=$(SHARED_SRC) $(PROGRAM_SRC)
DEPS:=$(ALL_SRC:%=$(BUILD_DIR)/%.dep)

LINUX_SHARED_OBJECTS:=$(SHARED_SRC:%=$(BUILD_DIR)/%.o)
MINGW_SHARED_OBJECTS:=$(SHARED_SRC:%=$(BUILD_DIR)/%_.o)

LINUX_OBJECTS:=$(LINUX_SHARED_OBJECTS) $(PROGRAM_SRC:%=$(BUILD_DIR)/%.o)
MINGW_OBJECTS:=$(MINGW_SHARED_OBJECTS) $(PROGRAM_SRC:%=$(BUILD_DIR)/%_.o)

LINUX_PROGRAMS:=$(PROGRAM_SRC:%=%)
MINGW_PROGRAMS:=$(PROGRAM_SRC:%=%.exe)
HTML5_PROGRAMS:=$(PROGRAM_SRC:%=%.html)
HTML5_PROGRAMS_SRC:=$(PROGRAM_SRC:%=%.html.cpp)

all: lib/libfwk.a lib/libfwk_win32.a lib/libfwk.cpp $(LINUX_PROGRAMS) $(MINGW_PROGRAMS)

LINUX_AR =ar
LINUX_STRIP=strip
LINUX_PKG_CONFIG=pkg-config

MINGW_CXX=$(MINGW_PREFIX)g++
MINGW_STRIP=$(MINGW_PREFIX)strip
MINGW_AR=$(MINGW_PREFIX)ar
MINGW_PKG_CONFIG=$(MINGW_PREFIX)pkg-config

LIBS=freetype2 sdl2 libpng zlib libmpg123
LINUX_LIBS=$(shell $(LINUX_PKG_CONFIG) --libs $(LIBS)) -lopenal -lGL -lGLU -lrt -fopenmp 
MINGW_LIBS=$(shell $(MINGW_PKG_CONFIG) --libs $(LIBS)) -lOpenAL32 -ldsound -lole32 -lwinmm -lglu32 -lopengl32 -lws2_32

INCLUDES=-Iinclude/ -Isrc/

NICE_FLAGS=-std=c++14 -Wall -Woverloaded-virtual -Wnon-virtual-dtor -Werror=return-type -Wno-reorder -Wuninitialized -Wno-unused-function \
		   -Wno-unused-variable -Wparentheses -Wno-overloaded-virtual #-Werror
LINUX_FLAGS=-DFWK_TARGET_LINUX -ggdb $(shell $(LINUX_PKG_CONFIG) --cflags $(LIBS)) -Umain $(NICE_FLAGS) $(INCLUDES) $(FLAGS)
MINGW_FLAGS=-DFWK_TARGET_MINGW -ggdb $(shell $(MINGW_PKG_CONFIG) --cflags $(LIBS)) -Umain $(NICE_FLAGS) $(INCLUDES) $(FLAGS)
HTML5_FLAGS=-DFWK_TARGET_HTML5 --memory-init-file 0 -O2 -s USE_SDL=2 -s USE_LIBPNG=1 -s USE_ZLIB=1 $(NICE_FLAGS) $(INCLUDES)

$(DEPS): $(BUILD_DIR)/%.dep: src/%.cpp
	$(LINUX_CXX) $(LINUX_FLAGS) -MM $< -MT $(BUILD_DIR)/$*.o   > $@
	$(MINGW_CXX) $(MINGW_FLAGS) -MM $< -MT $(BUILD_DIR)/$*_.o >> $@

$(LINUX_OBJECTS): $(BUILD_DIR)/%.o:  src/%.cpp
	$(LINUX_CXX) $(LINUX_FLAGS) -c src/$*.cpp -o $@

$(MINGW_OBJECTS): $(BUILD_DIR)/%_.o: src/%.cpp
	$(MINGW_CXX) $(MINGW_FLAGS) -c src/$*.cpp -o $@

$(LINUX_PROGRAMS): %:     $(LINUX_SHARED_OBJECTS) $(BUILD_DIR)/%.o
	$(LINUX_CXX) -o $@ $^ -rdynamic $(LINUX_LIBS) $(LIBS_$@)

$(MINGW_PROGRAMS): %.exe: $(MINGW_SHARED_OBJECTS) $(BUILD_DIR)/%_.o
	$(MINGW_CXX) -o $@ $^ $(MINGW_LIBS) $(LIBS_$*)
	$(MINGW_STRIP) $@

$(HTML5_PROGRAMS_SRC): %.html.cpp: src/%.cpp $(SHARED_SRC:%=src/%.cpp)
	cat $^ > $@

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

clean:
	-rm -f $(LINUX_OBJECTS) $(MINGW_OBJECTS) $(LINUX_PROGRAMS) $(MINGW_PROGRAMS) \
		$(HTML5_PROGRAMS) $(HTML5_PROGRAMS_SRC) $(HTML5_PROGRAMS:%.html=%.js) \
		$(DEPS) $(BUILD_DIR)/.depend lib/libfwk.a lib/libfwk_win32.a lib/libfwk.cpp lib/libfwk.html.cpp
	-rmdir test temp lib tools
	find $(BUILD_DIR) -type d -empty -delete

$(BUILD_DIR)/.depend: $(DEPS)
	cat $(DEPS) > $(BUILD_DIR)/.depend

depend: $(BUILD_DIR)/.depend

.PHONY: clean depend

DEPEND_FILE=$(BUILD_DIR)/.depend
DEP=$(wildcard $(DEPEND_FILE))
ifneq "$(DEP)" ""
include $(DEP)
endif

