# Requires llvm & libclang 8

BUILD_DIR=../../build/checker
MINGW_PREFIX=i686-w64-mingw32.static-
LINUX_CXX=clang++
FLAGS=

LLVM_DIR=/usr/lib/llvm-8

ALL=../../checker.so

-include Makefile.local

_dummy := $(shell [ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR))

SHARED_SRC=
PLUGIN_SRC=checker

ALL_SRC=$(PROGRAM_SRC) $(PLUGIN_SRC)

DEPS:=$(ALL_SRC:%=$(BUILD_DIR)/%.dep)

LINUX_OBJECTS:=$(ALL_SRC:%=$(BUILD_DIR)/%.o)
LINUX_PLUGIN_OBJECTS:=$(PLUGIN_SRC:%=$(BUILD_DIR)/%.o)
LINUX_PLUGINS:=$(PLUGIN_SRC:%=../../%.so)

all: $(LINUX_PROGRAMS) $(LINUX_SHARED_LIBS)

LLVM_CONFIG=$(LLVM_DIR)/llvm-config

PCH_FILE_SRC=pch.h

PCH_FILE_H=$(BUILD_DIR)/pch.h
PCH_FILE_PCH=$(BUILD_DIR)/pch.h.pch

PCH_INCLUDE=-include-pch $(PCH_FILE_PCH)
PCH_FILE_MAIN=$(PCH_FILE_PCH)

$(PCH_FILE_H): $(PCH_FILE_SRC)
	cp $^ $@
$(PCH_FILE_MAIN): $(PCH_FILE_H)
	$(LINUX_CXX) -x c++-header -MMD $(PCH_FILE_H) -o $@

#LLVM_LIBS=-lclangFrontendTool -lclangFrontend -lclangDriver -lclangSerialization -lclangCodeGen -lclangParse \
		  -lclangSema -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore \
		  -lclangAnalysis -lclangARCMigrate -lclangEdit -lclangASTMatchers -lclangAST -lclangLex -lclangBasic \
		  `$(LLVM_CONFIG) --libs --system-libs`

LINUX_LIBS=-L$(LLVM_DIR)/lib

INCLUDES=-I$(LLVM_DIR)/include -I./

NICE_FLAGS=-fno-rtti -Wall -Wextra -Woverloaded-virtual -Wnon-virtual-dtor -Werror=return-type -Wno-reorder \
		   -Wuninitialized -Wno-unused-function -Werror=switch -Wno-unused-variable -Wno-unused-parameter \
		   -Wparentheses -Wno-overloaded-virtual #-Werror

FLAGS+=-std=c++1z -ggdb -Wall -pthread -fPIC $(NICE_FLAGS) $(INCLUDES)
LINUX_FLAGS=$(FLAGS)

LINUX_STRIP=strip

$(DEPS): $(BUILD_DIR)/%.dep: %.cpp $(PCH_FILE_H).d
	-$(LINUX_CXX) $(LINUX_FLAGS) -MM $< -MT $(BUILD_DIR)/$*.o   > $@

$(LINUX_OBJECTS): $(BUILD_DIR)/%.o:  %.cpp $(PCH_FILE_MAIN)
	$(LINUX_CXX) $(LINUX_FLAGS) $(FLAGS_$*) $(PCH_INCLUDE) -c $*.cpp -o $@

$(LINUX_PLUGINS): ../../%.so:  $(LINUX_PLUGIN_OBJECTS) $(BUILD_DIR)/%.o
	    $(LINUX_CXX) -shared -o $@ $^ $(LIBS_$@) $(LINUX_LIBS)

clean:
	-rm -f $(LINUX_PLUGINS) $(LINUX_PLUGIN_OBJECTS) $(DEPS) $(BUILD_DIR)/.depend
	-rmdir output
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

