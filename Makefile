# Options:
#   All the options from Makefile-shared are available except FWK_MODE
#   FWK_MERGE_MODULES: modules composed of multiple .cpp files are built together; this improves
#                      whole builds (but incremental builds are slower).
#                      When changing this option you might have to remove libfwk.a, otherwise you could
#                      get multiple defined symbols error.

# Special targets:
#  All targets from Makefile-shared are also available
#  clean-checker:   removes build files for checker
#  checker.so:      EXCEPT checker (will be automatically used during builds)

# TODO: niezależne katalogi linux-gcc linux-clang ?
# TODO: uzależnić *_merged.cpp od listy plików a nie całego makefile-a

all: lib tools tests

CFLAGS_linux  = $(shell $(PKG_CONFIG) --cflags $(LIBS_shared)) -Umain
CFLAGS_mingw  = $(shell $(PKG_CONFIG) --cflags $(LIBS_shared)) -Umain
CFLAGS        = -Isrc/ -Iextern/imgui/ -Idependencies/include/
BUILD_DIR     = $(FWK_BUILD_DIR)
PCH_SOURCE   := src/fwk_pch.h

FWK_DIR  = 
include Makefile-shared

# --- Creating necessary sub-directories ----------------------------------------------------------

SUBDIRS        = build tests tools lib temp
BUILD_SUBDIRS  = tests tools
ifndef FWK_MERGE_MODULES
BUILD_SUBDIRS += gfx audio math sys io tests tools gui perf geom vulkan
endif

ifndef JUNK_GATHERING
_dummy := $(shell mkdir -p $(SUBDIRS))
_dummy := $(shell mkdir -p $(addprefix $(FWK_BUILD_DIR)/,$(BUILD_SUBDIRS)))
endif

# --- Lists of source files -----------------------------------------------------------------------

SRC_base = base_vector hash_map_stats enum str type_info any logger any_config format parse slab_allocator bit_vector

SRC_sys  = \
	sys_base sys/error sys/exception sys/thread sys/expected sys/assert sys/assert_impl sys/on_fail \
	sys/memory sys/backtrace sys/input \
	io/xml io/stream io/file_system io/file_stream io/memory_stream io/gzip_stream io/package_file \
	io/url_fetch

ifeq ($(FWK_DWARF), enabled)
	SRC_sys += sys/backtrace_dwarf
endif

SRC_math = \
	math/cylinder math/box math/obox math/frustum math/matrix3 math/matrix4 math/matrix4_transform math/plane \
	math/quat math/base math/triangle math/tetrahedron math/projection math/random math/segment math/line \
	math/affine_trans math/rational math/gcd math/rational_angle math/qint math/rotation math/ray

ifeq ($(PLATFORM), html)
	SRC_math += math/int128 math/uint128
endif

SRC_gfx = \
	gfx/camera_control gfx/camera gfx/canvas_2d gfx/canvas_3d gfx/color gfx/colored_quad gfx/colored_triangle \
	gfx/drawing gfx/dynamic_mesh gfx/font gfx/font_factory gfx/font_finder \
	gfx/fpp_camera gfx/image gfx/image_tga gfx/investigate gfx/investigator2 gfx/investigator3 \
	gfx/matrix_stack gfx/orbiting_camera gfx/ortho_camera gfx/plane_camera gfx/shader_compiler \
	gfx/shader_debug gfx/shader_defs gfx/shader_reflection

SRC_gfx_mesh = \
	gfx/animated_model gfx/converter gfx/mesh_buffers gfx/mesh_constructor gfx/mesh gfx/mesh_indices \
	gfx/model_anim gfx/model gfx/model_node gfx/pose

SRC_vulkan = \
	vulkan/vulkan_buffer vulkan/vulkan_command_queue vulkan/vulkan_descriptor_manager vulkan/vulkan_device \
	vulkan/vulkan_framebuffer vulkan/vulkan_image vulkan/vulkan_instance vulkan/vulkan_internal \
	vulkan/vulkan_memory_manager vulkan/vulkan_pipeline vulkan/vulkan_shader vulkan/vulkan_storage \
	vulkan/vulkan_swap_chain vulkan/vulkan_window vulkan/vulkan_ray_tracing vulkan/vulkan_render_pass vulkan_base

SRC_gfx_stbi = \
	gfx/image_stbi gfx/image_stbir

SRC_audio = audio/al_device audio/sound audio/ogg_stream

ifeq ($(FWK_GEOM), enabled)
SRC_geom = geom_base geom/contour geom/regular_grid geom/segment_grid geom/procgen
SRC_geom_graph = geom/element_ref geom/graph geom/geom_graph
SRC_geom_voronoi = geom/voronoi geom/wide_int geom/voronoi_constructor geom/delaunay
endif

ifeq ($(FWK_IMGUI), enabled)
SRC_gui_imgui1 = gui/imgui_base
SRC_gui_imgui2 = gui/imgui_draw
SRC_gui_imgui3 = gui/imgui_widgets gui/imgui_tables
SRC_gui_imgui4 = gui/imgui_demo
SRC_gui = gui/gui gui/popups gui/widgets perf/analyzer
endif

SRC_perf = perf/perf_base perf/exec_tree perf/manager perf/thread_context

SRC_tests = \
	tests/stuff tests/math tests/geom tests/window tests/vector_perf tests/variant_perf tests/hash_map_perf
SRC_tools = tools/model_viewer tools/packager

ifneq ($(PLATFORM), html)
SRC_tests += tests/models
SRC_tools += tools/model_convert
endif

WEBGL_PROGRAMS := tests/window tools/model_viewer
SRC_programs    = $(SRC_tests) $(SRC_tools)

MODULES = gui_imgui1 gui_imgui2 gui_imgui3 gui_imgui4 base sys gfx gfx_mesh gfx_stbi vulkan \
		  math geom geom_graph geom_voronoi gui perf audio

# --- Definitions ---------------------------------------------------------------------------------

SRC_merged = $(MODULES:%=%_merged)
SRC_shared = $(foreach MODULE,$(MODULES),$(SRC_$(MODULE)))
SRC_all    = $(SRC_merged) $(SRC_shared) $(SRC_programs)

CPP_merged = $(SRC_merged:%=$(FWK_BUILD_DIR)/%.cpp)
CPP_shared = $(SRC_shared:%=src/%.cpp)

SHARED_OBJECTS := $(SRC_shared:%=$(FWK_BUILD_DIR)/%.o)
MERGED_OBJECTS := $(SRC_merged:%=$(FWK_BUILD_DIR)/%.o)
OBJECTS        := $(SHARED_OBJECTS) $(SRC_programs:%=$(FWK_BUILD_DIR)/%.o)

INPUT_OBJECTS  := $(if $(FWK_MERGE_MODULES),$(MERGED_OBJECTS),$(SHARED_OBJECTS))
INPUT_SRCS     := $(if $(FWK_MERGE_MODULES),$(CPP_merged),$(CPP_shared))

PROGRAMS       := $(SRC_programs:%=%$(PROGRAM_SUFFIX))

ifeq ($(PLATFORM), html)
PROGRAMS_JUNK  := $(SRC_programs:%=%.wasm) $(SRC_programs:%=%.js)
LDFLAGS        += -s TOTAL_MEMORY=256MB --preload-file data/ #TODO: load only font
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
ifeq ($(PLATFORM), html)
	$(LINKER) -MMD -o $@ $^ $(LDFLAGS) --shell-file src/html_$(if $(findstring $*,$(WEBGL_PROGRAMS)),webgl,text).html
else
	$(LINKER) -MMD -o $@ $^ $(LDFLAGS)
endif

ifeq ($(COMPILER_TYPE),clang)
checker.so: .FORCE
	$(MAKE) -C src/checker/ ../../checker.so
endif

$(FWK_LIB_FILE): $(INPUT_OBJECTS)
	$(ARCHIVER) r $@ $^

DEPS:=$(SRC_all:%=$(FWK_BUILD_DIR)/%.d) $(PCH_TEMP).d


# --- Clean targets -------------------------------------------------------------------------------

JUNK_FILES := $(OBJECTS) $(DEPS) $(MERGED_OBJECTS) $(PROGRAMS) $(PROGRAMS_JUNK) $(CPP_merged) $(FWK_LIB_FILE)
JUNK_DIRS  := $(SUBDIRS)

clean-all: clean-checker
clean-checker:
	$(MAKE) -C src/checker/ clean

# --- Other stuff ---------------------------------------------------------------------------------

.PHONY: tools tests lib clean-checker

ifndef JUNK_GATHERING
-include $(DEPS)
endif
