MINGW_PREFIX=i686-w64-mingw32.static-
BUILD_DIR=build

-include Makefile.local

_dummy := $(shell [ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR))
_dummy := $(shell [ -d test ] || mkdir -p test)
_dummy := $(shell [ -d lib ] || mkdir -p lib)
_dummy := $(shell [ -d $(BUILD_DIR)/gfx ] || mkdir -p $(BUILD_DIR)/gfx)
_dummy := $(shell [ -d $(BUILD_DIR)/math ] || mkdir -p $(BUILD_DIR)/math)
_dummy := $(shell [ -d $(BUILD_DIR)/test ] || mkdir -p $(BUILD_DIR)/test)


SHARED_SRC=base filesystem input profiler stream xml xml_conversions \
		   gfx/color gfx/device gfx/device_texture gfx/font gfx/font_factory \
		   gfx/opengl gfx/texture gfx/texture_format gfx/texture_tga gfx/collada gfx/mesh \
		   gfx/vertex_array gfx/vertex_buffer gfx/shader gfx/program gfx/renderer \
		   math/box math/frustum math/matrix3 math/matrix4 math/plane math/ray math/rect math/vector
PROGRAM_SRC=test/streams test/stuff test/math test/window
LINUX_SRC=filesystem_linux
MINGW_SRC=filesystem_windows
HTML5_SRC=filesystem_linux


LINUX_SHARED_SRC=$(SHARED_SRC) $(LINUX_SRC)
MINGW_SHARED_SRC=$(SHARED_SRC) $(MINGW_SRC)
ALL_SRC=$(SHARED_SRC) $(PROGRAM_SRC) $(LINUX_SRC) $(MINGW_SRC)
DEPS:=$(ALL_SRC:%=$(BUILD_DIR)/%.dep)

LINUX_SHARED_OBJECTS:=$(LINUX_SHARED_SRC:%=$(BUILD_DIR)/%.o)
MINGW_SHARED_OBJECTS:=$(MINGW_SHARED_SRC:%=$(BUILD_DIR)/%_.o)

LINUX_OBJECTS:=$(LINUX_SHARED_OBJECTS) $(PROGRAM_SRC:%=$(BUILD_DIR)/%.o)
MINGW_OBJECTS:=$(MINGW_SHARED_OBJECTS) $(PROGRAM_SRC:%=$(BUILD_DIR)/%_.o)

LINUX_PROGRAMS:=$(PROGRAM_SRC:%=%)
MINGW_PROGRAMS:=$(PROGRAM_SRC:%=%.exe)
HTML5_PROGRAMS:=$(PROGRAM_SRC:%=%.html)
HTML5_PROGRAMS_SRC:=$(PROGRAM_SRC:%=%.html.cpp)

all: lib/libfwk.a lib/libfwk_win32.a $(LINUX_PROGRAMS) $(MINGW_PROGRAMS)

LINUX_CXX=g++
LINUX_AR =ar
LINUX_STRIP=strip
LINUX_PKG_CONFIG=pkg-config

MINGW_CXX=$(MINGW_PREFIX)g++
MINGW_STRIP=$(MINGW_PREFIX)strip
MINGW_AR=$(MINGW_PREFIX)ar
MINGW_PKG_CONFIG=$(MINGW_PREFIX)pkg-config

LIBS=freetype2 sdl libpng zlib libmpg123
LINUX_LIBS=$(shell $(LINUX_PKG_CONFIG) --libs $(LIBS)) -lopenal -lGL -lGLU -lrt -fopenmp 
MINGW_LIBS=$(shell $(MINGW_PKG_CONFIG) --libs $(LIBS)) -lOpenAL32 -ldsound -lole32 -lwinmm -lglu32 -lopengl32 -lws2_32

INCLUDES=-Iinclude/ -Isrc/

NICE_FLAGS=-std=c++11 -ggdb -Wall -Woverloaded-virtual -Wnon-virtual-dtor -Werror=return-type -Wno-reorder -Wno-uninitialized -Wno-unused-function \
		   -Wno-unused-variable -Wparentheses -Wno-overloaded-virtual #-Werror
LINUX_FLAGS=-DFWK_TARGET_LINUX $(shell $(LINUX_PKG_CONFIG) --cflags $(LIBS)) $(NICE_FLAGS) $(INCLUDES) $(FLAGS) -rdynamic
MINGW_FLAGS=-DFWK_TARGET_MINGW $(shell $(MINGW_PKG_CONFIG) --cflags $(LIBS)) $(NICE_FLAGS) $(INCLUDES) $(FLAGS)
HTML5_FLAGS=-DFWK_TARGET_HTML5 -std=c++11 -lSDL -O0 $(INCLUDES)

$(DEPS): $(BUILD_DIR)/%.dep: src/%.cpp
	$(LINUX_CXX) $(LINUX_FLAGS) -MM $< -MT $(BUILD_DIR)/$*.o   > $@
	$(MINGW_CXX) $(MINGW_FLAGS) -MM $< -MT $(BUILD_DIR)/$*_.o >> $@

$(LINUX_OBJECTS): $(BUILD_DIR)/%.o:  src/%.cpp
	$(LINUX_CXX) $(LINUX_FLAGS) -c src/$*.cpp -o $@

$(MINGW_OBJECTS): $(BUILD_DIR)/%_.o: src/%.cpp
	$(MINGW_CXX) $(MINGW_FLAGS) -c src/$*.cpp -o $@

$(LINUX_PROGRAMS): %:     $(LINUX_SHARED_OBJECTS) $(BUILD_DIR)/%.o
	$(LINUX_CXX) -o $@ $^ -rdynamic $(LIBS_$@) $(LINUX_LIBS)

$(MINGW_PROGRAMS): %.exe: $(MINGW_SHARED_OBJECTS) $(BUILD_DIR)/%_.o
	$(MINGW_CXX) -o $@ $^  $(LIBS_$*) $(MINGW_LIBS)
	$(MINGW_STRIP) $@

$(HTML5_PROGRAMS_SRC): %.html.cpp: src/%.cpp $(SHARED_SRC:%=src/%.cpp) $(HTML5_SRC:%=src/%.cpp)
	cat $^ > $@

$(HTML5_PROGRAMS): %.html: %.html.cpp
	emcc $(HTML5_FLAGS) $^ -o $@


lib/libfwk.a: $(LINUX_SHARED_OBJECTS)
	$(LINUX_AR) r $@ $^ 

lib/libfwk_win32.a: $(MINGW_SHARED_OBJECTS)
	$(MINGW_AR) r $@ $^ 

lib/libfwk.html.cpp: $(SHARED_SRC:%=src/%.cpp) $(HTML5_SRC:%=src/%.cpp)
	cat $^ > $@

clean:
	-rm -f $(LINUX_OBJECTS) $(MINGW_OBJECTS) $(LINUX_PROGRAMS) $(MINGW_PROGRAMS) \
		$(HTML5_PROGRAMS) $(HTML5_PROGRAMS_SRC) $(HTML5_PROGRAMS:%.html=%.js) \
		$(DEPS) $(BUILD_DIR)/.depend lib/libfwk.a lib/libfwk_win32.a lib/libfwk.html.cpp
	-rmdir test lib $(BUILD_DIR)/test $(BUILD_DIR)/gfx $(BUILD_DIR)/math
	-rmdir $(BUILD_DIR)

$(BUILD_DIR)/.depend: $(DEPS)
	cat $(DEPS) > $(BUILD_DIR)/.depend

depend: $(BUILD_DIR)/.depend

.PHONY: clean depend

DEPEND_FILE=$(BUILD_DIR)/.depend
DEP=$(wildcard $(DEPEND_FILE))
ifneq "$(DEP)" ""
include $(DEP)
endif

