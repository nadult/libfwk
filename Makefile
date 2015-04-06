MINGW_PREFIX=i686-w64-mingw32.static-
BUILD_DIR=build

-include Makefile.local

_dummy := $(shell [ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR))
_dummy := $(shell [ -d test ] || mkdir -p test)
_dummy := $(shell [ -d lib ] || mkdir -p lib)
_dummy := $(shell [ -d $(BUILD_DIR)/gfx ] || mkdir -p $(BUILD_DIR)/gfx)
_dummy := $(shell [ -d $(BUILD_DIR)/test ] || mkdir -p $(BUILD_DIR)/test)


SHARED_SRC=base filesystem input math profiler stream xml \
		   gfx/color gfx/device gfx/device_texture gfx/drawing gfx/font gfx/font_factory \
		   gfx/opengl gfx/texture gfx/texture_format gfx/texture_tga 
PROGRAM_SRC=test/streams test/stuff
LINUX_SRC=filesystem_linux
MINGW_SRC=filesystem_windows


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

all: lib/libfwk.a lib/libfwk_win32.a $(LINUX_PROGRAMS) $(MINGW_PROGRAMS)

LINUX_CXX=g++
LINUX_AR =ar
LINUX_STRIP=strip
LINUX_PKG_CONFIG=pkg-config

MINGW_CXX=$(MINGW_PREFIX)g++
MINGW_STRIP=$(MINGW_PREFIX)strip
MINGW_AR=$(MINGW_PREFIX)ar
MINGW_PKG_CONFIG=$(MINGW_PREFIX)pkg-config

LIBS=-lglfw -lpng -lz -lmpg123 
LINUX_LIBS=$(LIBS) `$(LINUX_PKG_CONFIG) --libs freetype2` -lopenal -lGL -lGLU -lrt -fopenmp 
MINGW_LIBS=$(LIBS) `$(MINGW_PKG_CONFIG) --libs freetype2` -lOpenAL32 -ldsound -lole32 -lwinmm -lglu32 -lopengl32 -lws2_32

INCLUDES=-Iinclude/ -Isrc/

NICE_FLAGS=-Woverloaded-virtual -Wnon-virtual-dtor -Werror=return-type -Wno-reorder -Wno-uninitialized -Wno-unused-function \
		   -Wno-unused-but-set-variable -Wno-unused-variable -Wparentheses -Wno-overloaded-virtual #-Werror
FLAGS=-std=c++11 -O0 -ggdb -Wall $(NICE_FLAGS) $(INCLUDES)
LINUX_FLAGS=`$(LINUX_PKG_CONFIG) --cflags freetype2` $(FLAGS) -rdynamic
MINGW_FLAGS=`$(MINGW_PKG_CONFIG) --cflags freetype2` $(FLAGS)


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

lib/libfwk.a: $(LINUX_SHARED_OBJECTS)
	$(LINUX_AR) r $@ $^ 

lib/libfwk_win32.a: $(MINGW_SHARED_OBJECTS)
	$(MINGW_AR) r $@ $^ 

clean:
	-rm -f $(LINUX_OBJECTS) $(MINGW_OBJECTS) $(LINUX_PROGRAMS) $(MINGW_PROGRAMS) \
		$(DEPS) $(BUILD_DIR)/.depend lib/libfwk.a lib/libfwk_win32.a
	-rmdir test lib $(BUILD_DIR)/test $(BUILD_DIR)/gfx
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

