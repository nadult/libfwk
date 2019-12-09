# Options:
#   All the options from Makefile-shared are available except FWK_MODE
#   STATS:           gathers build times for each object file
#   SPLIT_MODULES:   normally modules composed of multiple .cpp files are built together; this
#                    option forces separate compilation for each .cpp file
#   MINGW_PREFIX:    prefix for mingw toolset; Can be set in the environment
#
# Developer options:
#   INTROSPECT_MODE: does not create sub-directories and does not include .d files

# Special targets:
#  print-variables: prints contents of main variables
#  print-stats:     prints build stats (generated with STATS option)
#  clean-checker:   removes build files for checker
#  clean-all:       clears all possible targets (including checker)

# TODO: niezależne katalogi linux-gcc linux-clang ?
# TODO: uzależnić *_merged.cpp od listy plików a nie całego makefile-a

all: lib tools tests

CFLAGS_linux  = $(shell $(PKG_CONFIG) --cflags $(LIBS_shared)) -Umain
CFLAGS_mingw  = $(shell $(PKG_CONFIG) --cflags $(LIBS_shared)) -Umain
CFLAGS        = -Isrc/ -DFATAL=FWK_FATAL -DDUMP=FWK_DUMP
BUILD_DIR     = $(FWK_BUILD_DIR)
PCH_SOURCE   := src/fwk_pch.h

FWK_DIR  = 
include Makefile-shared

# --- Creating necessary sub-directories ----------------------------------------------------------


SUBDIRS        = build tests tools lib temp
BUILD_SUBDIRS  = tests tools
ifdef SPLIT_MODULES
BUILD_SUBDIRS += gfx audio math sys tests tools menu perf geom
endif

ifndef INTROSPECT_MODE
_dummy := $(shell mkdir -p $(SUBDIRS))
_dummy := $(shell mkdir -p $(addprefix $(FWK_BUILD_DIR)/,$(BUILD_SUBDIRS)))
endif

# --- Lists of source files -----------------------------------------------------------------------

SRC_base = base_vector hash_map_stats enum str type_info any logger any_config format parse

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

ifeq ($(FWK_GEOM), enabled)
SRC_geom = geom_base geom/contour geom/regular_grid geom/segment_grid geom/procgen
SRC_geom_graph = geom/element_ref geom/graph geom/geom_graph
SRC_geom_voronoi = geom/voronoi geom/wide_int geom/voronoi_constructor geom/delaunay
endif

ifeq ($(FWK_IMGUI), enabled)
SRC_menu_imgui = menu/imgui_code
SRC_menu = menu/open_file_popup menu/error_popup menu/helpers menu/imgui_wrapper perf/analyzer
endif

SRC_perf = perf/perf_base perf/exec_tree perf/manager perf/thread_context

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

CPP_merged = $(SRC_merged:%=$(FWK_BUILD_DIR)/%.cpp)
CPP_shared = $(SRC_shared:%=src/%.cpp)

SHARED_OBJECTS := $(SRC_shared:%=$(FWK_BUILD_DIR)/%.o)
MERGED_OBJECTS := $(SRC_merged:%=$(FWK_BUILD_DIR)/%.o)
OBJECTS        := $(SHARED_OBJECTS) $(SRC_programs:%=$(FWK_BUILD_DIR)/%.o)

INPUT_OBJECTS  := $(if $(SPLIT_MODULES),$(SHARED_OBJECTS),$(MERGED_OBJECTS))
INPUT_SRCS     := $(if $(SPLIT_MODULES),$(CPP_shared),$(CPP_merged))

PROGRAMS       := $(SRC_programs:%=%$(PROGRAM_SUFFIX))
#HTML5_PROGRAMS_SRC:=$(SRC_programs:%=%.html.cpp)

STATS_FILE = $(FWK_BUILD_DIR)/stats.txt
ifdef STATS
STATS_CMD  = time -o $(STATS_FILE) -a -f "%U $*.o"
endif

lib:   $(FWK_LIB_FILE)
tools: $(SRC_tools:%=%$(PROGRAM_SUFFIX))
tests: $(SRC_tests:%=%$(PROGRAM_SUFFIX))

# --- Main build targets --------------------------------------------------------------------------

$(CPP_merged): $(FWK_BUILD_DIR)/%_merged.cpp: Makefile $(CONFIG_FILE)
	@echo "$(SRC_$*:%=#include \"%.cpp\"\n)" > $@

$(OBJECTS): $(FWK_BUILD_DIR)/%.o: src/%.cpp $(PCH_TARGET)
	$(STATS_CMD) $(COMPILER) -MMD $(CFLAGS) $(PCH_CFLAGS) -c src/$*.cpp -o $@

$(MERGED_OBJECTS): $(FWK_BUILD_DIR)/%.o: $(FWK_BUILD_DIR)/%.cpp $(PCH_TARGET)
	$(STATS_CMD) $(COMPILER) -MMD $(CFLAGS) $(PCH_CFLAGS) -c $(FWK_BUILD_DIR)/$*.cpp -o $@

$(PROGRAMS): %$(PROGRAM_SUFFIX): $(INPUT_OBJECTS) $(FWK_BUILD_DIR)/%.o
	$(LINKER) -MMD -o $@ $^ $(LDFLAGS)

ifeq ($(COMPILER_TYPE),clang)
checker.so: .FORCE
	$(MAKE) -C src/checker/ ../../checker.so
endif

#$(HTML5_PROGRAMS_SRC): %.html.cpp: src/%.cpp $(SRC_shared:%=src/%.cpp)
#	cat src/html_pre_include.cpp $^ > $@
#$(HTML5_PROGRAMS): %.html: %.html.cpp
#	emcc $(HTML5_FLAGS) $^ -o $@

$(FWK_LIB_FILE): $(INPUT_OBJECTS)
	$(ARCHIVER) r $@ $^

#lib/libfwk.cpp: $(SRC_shared:%=src/%.cpp)
# TODO: catting sux
#	cat $^ > $@
#lib/libfwk.html.cpp: $(SRC_shared:%=src/%.cpp) $(HTML5_SRC:%=src/%.cpp)
#	cat $^ > $@

DEPS:=$(SRC_all:%=$(FWK_BUILD_DIR)/%.d) $(PCH_TEMP).d

print-stats:
	sort -n -r $(STATS_FILE)

# --- Clean targets -------------------------------------------------------------------------------

JUNK_FILES = $(OBJECTS) $(DEPS) $(MERGED_OBJECTS) $(PROGRAMS) $(CPP_merged) $(STATS_FILE) \
		$(FWK_LIB_FILE) $(PCH_JUNK)
EXISTING_JUNK_FILES := $(call filter-existing,$(SUBDIRS),$(JUNK_FILES))
print-junk-files:
	@echo $(EXISTING_JUNK_FILES)
ALL_JUNK_FILES = $(sort $(shell \
	for platform in $(VALID_PLATFORMS) ; do for mode in $(VALID_MODES) ; do \
		$(MAKE) PLATFORM=$$platform MODE=$$mode print-junk-files INTROSPECT_MODE=1 -s ; \
	done ; done))

clean:
	rm $(sort $(EXISTING_JUNK_FILES))
	find $(SUBDIRS) -type d -empty -delete

clean-all: clean-checker
	rm $(ALL_JUNK_FILES)
	find $(SUBDIRS) -type d -empty -delete

clean-checker:
	$(MAKE) -C src/checker/ clean

# --- Other stuff ---------------------------------------------------------------------------------

print-variables:
	@echo "PLATFORM      = $(PLATFORM)"
	@echo "MODE          = $(MODE)"
	@echo "COMPILER      = $(COMPILER)"
	@echo "LINKER        = $(LINKER)"
	@echo "COMPILER_TYPE = $(COMPILER_TYPE)"
	@echo
	@echo "FWK_BUILD_DIR = $(FWK_BUILD_DIR)"
	@echo "FWK_LIB_FILE  = $(FWK_LIB_FILE)"
	@echo 
	@echo "PCH_TARGET = $(PCH_TARGET)"
	@echo "PCH_CFLAGS = $(PCH_CFLAGS)"
	@echo 
	@echo "FWK_IMGUI     = $(FWK_IMGUI)"
	@echo "FWK_GEOM      = $(FWK_GEOM)"
	@echo 
	@echo "CFLAGS  = $(CFLAGS)"
	@echo 
	@echo "LDFLAGS = $(LDFLAGS)"

.PHONY: clean tools tests lib clean-all clean-checker print-junk-files print-stats

ifndef INTROSPECT_MODE
-include $(DEPS)
endif
