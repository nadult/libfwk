# CMakeLists.txt for libfwk library

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	set(CMAKE_GENERATOR_TOOLSET "ClangCL")
endif()

# Set custom configuration types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Develop" CACHE STRING "" FORCE)

# Configuration-specific compiler flags
if (CMAKE_GENERATOR MATCHES "Visual Studio")
	# MSVC flags
	set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /MDd /RTC1 /D_DEBUG /DFWK_DEBUG"
	    CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /MD /DNDEBUG /DFWK_RELEASE"
	    CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_DEVELOP "/Zi /O2 /MD /DFWK_DEVELOP"
	    CACHE STRING "" FORCE)

	set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_DEVELOP ${CMAKE_CXX_FLAGS_DEVELOP} CACHE STRING "" FORCE)
else()
	# GCC/Clang flags
	set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG /DFWK_DEBUG" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG /DFWK_RELEASE" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_DEVELOP "-g -O2 -DDEVELOP /DFWK_DEVELOP" CACHE STRING "" FORCE)

	set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_DEVELOP ${CMAKE_CXX_FLAGS_DEVELOP} CACHE STRING "" FORCE)
endif()

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Develop" CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "Develop")
endif()


project(libfwk)

# Options
option(FWK_MERGE_MODULES "Merge modules for faster full builds" OFF)
option(FWK_GEOM "Enable geometry modules" ON)
option(FWK_IMGUI "Enable ImGui support" ON)

# Include directories
include_directories(src/ extern/imgui/)

set(PCH_SOURCE "src/fwk_pch.h")
if(MSVC)
	# MSVC-specific PCH handling if needed
elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# GCC/Clang PCH handling
endif()

# Define source files for each module
set(SRC_base base_vector hash_map_stats enum str type_info any logger any_config format parse
             slab_allocator bit_vector)

set(SRC_sys
	sys_base sys/error sys/exception sys/thread sys/expected sys/assert sys/assert_impl sys/on_fail
	sys/memory sys/backtrace sys/input
	io/xml io/stream io/file_system io/file_stream io/memory_stream io/gzip_stream io/package_file
	io/url_fetch)

set(SRC_math
	math/cylinder math/box math/obox math/frustum math/matrix3 math/matrix4 math/matrix4_transform
	math/plane math/quat math/base math/triangle math/tetrahedron math/projection math/random
	math/segment math/line math/affine_trans math/rational math/gcd math/rational_angle math/qint
	math/rotation math/ray)

set(SRC_gfx
	gfx/camera_control gfx/camera gfx/canvas_2d gfx/canvas_3d gfx/color gfx/colored_quad
	gfx/colored_triangle gfx/drawing gfx/dynamic_mesh gfx/font gfx/font_factory gfx/font_finder
	gfx/fpp_camera gfx/image gfx/image_tga gfx/investigate gfx/investigator2 gfx/investigator3
	gfx/matrix_stack gfx/orbiting_camera gfx/ortho_camera gfx/plane_camera gfx/shader_compiler
	gfx/shader_debug gfx/shader_defs gfx/shader_reflection)

set(SRC_gfx_mesh
	gfx/animated_model gfx/converter gfx/mesh_buffers gfx/mesh_constructor gfx/mesh gfx/mesh_indices
	gfx/model_anim gfx/model gfx/model_node gfx/pose)

set(SRC_vulkan
	vulkan/vulkan_buffer vulkan/vulkan_command_queue vulkan/vulkan_descriptor_manager
	vulkan/vulkan_device vulkan/vulkan_framebuffer vulkan/vulkan_image vulkan/vulkan_instance
	vulkan/vulkan_internal vulkan/vulkan_memory_manager vulkan/vulkan_pipeline vulkan/vulkan_shader
	vulkan/vulkan_storage vulkan/vulkan_swap_chain vulkan/vulkan_window vulkan/vulkan_ray_tracing
	vulkan/vulkan_render_pass vulkan_base)

set(SRC_gfx_stbi
	gfx/image_stbi gfx/image_stbir)

set(SRC_audio
	audio/al_device audio/sound audio/ogg_stream)

if(FWK_GEOM)
	set(SRC_geom geom_base geom/contour geom/regular_grid geom/segment_grid geom/procgen)
	set(SRC_geom_graph geom/element_ref geom/graph geom/geom_graph)
	set(SRC_geom_voronoi geom/voronoi geom/wide_int geom/voronoi_constructor geom/delaunay)
endif()

if(FWK_IMGUI)
	set(SRC_gui_imgui1 gui/imgui_base)
	set(SRC_gui_imgui2 gui/imgui_draw)
	set(SRC_gui_imgui3 gui/imgui_widgets gui/imgui_tables)
	set(SRC_gui_imgui4 gui/imgui_demo)
	set(SRC_gui gui/gui gui/popups gui/widgets perf/analyzer)
endif()

set(SRC_perf
	perf/perf_base perf/exec_tree perf/manager perf/thread_context)

set(SRC_tests
	tests/stuff tests/math tests/geom tests/window tests/vector_perf tests/variant_perf
	tests/hash_map_perf tests/models)

set(SRC_tools
	tools/model_viewer tools/packager tools/model_convert)

# Collect all modules for the library
set(MODULES
	base sys math gfx gfx_mesh vulkan gfx_stbi audio perf)

function(get_module_sources MODULE OUTPUT_VARIABLE)
	set(MODULE_SOURCES "")
	foreach(SRC_FILE ${SRC_${MODULE}})
		list(APPEND MODULE_SOURCES "src/${SRC_FILE}.cpp")
	endforeach()
	set(${OUTPUT_VARIABLE} ${MODULE_SOURCES} PARENT_SCOPE)
endfunction()

foreach(MODULE ${MODULES})
	get_module_sources(${MODULE} MODULE_SOURCES)
	source_group("${MODULE}" FILES ${MODULE_SOURCES} )
	message(STATUS "Module ${MODULE} files: ${MODULE_SOURCES}")
endforeach()

if(FWK_GEOM)
	list(APPEND MODULES geom geom_graph geom_voronoi)
endif()

if(FWK_IMGUI)
	list(APPEND MODULES gui_imgui1 gui_imgui2 gui_imgui3 gui_imgui4 gui)
endif()


# Function to add prefix to list of files
function(prepend_path VAR PREFIX)
	set(result "")
	foreach(item ${${VAR}})
		list(APPEND result "${PREFIX}/${item}.cpp")
	endforeach()
	set(${VAR} ${result} PARENT_SCOPE)
endfunction()

# Build and link the library
if(FWK_MERGE_MODULES)
	# Create merged implementation
	foreach(MODULE ${MODULES})
		set(merged_file "${CMAKE_BINARY_DIR}/${MODULE}_merged.cpp")
		set_source_files_properties(${merged_file} PROPERTIES GENERATED TRUE)
		file(WRITE ${merged_file} "// Merged module: ${MODULE}\n")

		foreach(SRC_FILE ${SRC_${MODULE}})
			file(APPEND ${merged_file} "#include \"${SRC_FILE}.cpp\"\n")
		endforeach()

		list(APPEND LIB_SOURCES ${merged_file})
	endforeach()
else()
	# Use individual source files
	foreach(MODULE ${MODULES})
		prepend_path(SRC_${MODULE} "src")
		list(APPEND LIB_SOURCES ${SRC_${MODULE}})
	endforeach()
endif()

# Create the library
add_library(libfwk ${LIB_SOURCES})

target_include_directories(libfwk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_directories(libfwk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib)


message(STATUS "C++ Compiler Path: ${CMAKE_CXX_COMPILER}")

if(WIN32)
target_link_libraries(libfwk
	SDL3 dbghelp winmm version setupapi dwrite
	shaderc-combined-$<IF:$<CONFIG:Release>,md,mdd>
)
else()
target_link_libraries(libfwk
	SDL3 shaderc-combined)
endif()

# This is required for clang __int128 support on Windows
if ((CMAKE_GENERATOR MATCHES "Visual Studio") AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" CLANG_MAJOR_VERSION ${CMAKE_CXX_COMPILER_VERSION})
	get_filename_component(CLANG_MAIN_DIR_PATH "${CMAKE_CXX_COMPILER}/../.." ABSOLUTE)
	set(CLANG_RT_LIB_PATH "${CLANG_MAIN_DIR_PATH}/lib/clang/${CLANG_MAJOR_VERSION}/lib/windows/")
	target_link_libraries(libfwk ${CLANG_RT_LIB_PATH}/clang_rt.builtins-x86_64.lib)
	message(STATUS "PATH: ${CLANG_RT_LIB_PATH}")
endif()

# Add tests
foreach(TEST_SRC ${SRC_tests})
	string(REPLACE "/" "_" TEST_NAME ${TEST_SRC})
	prepend_path(CURRENT_TEST_SRC "src")
	set(CURRENT_TEST_SRC ${SRC_${TEST_SRC}})
	add_executable(${TEST_NAME} src/${TEST_SRC}.cpp)
	target_link_libraries(${TEST_NAME} libfwk)
endforeach()

# Add tools
foreach(TOOL_SRC ${SRC_tools})
	string(REPLACE "/" "_" TOOL_NAME ${TOOL_SRC})
	prepend_path(CURRENT_TOOL_SRC "src")
	set(CURRENT_TOOL_SRC ${SRC_${TOOL_SRC}})
	add_executable(${TOOL_NAME} src/${TOOL_SRC}.cpp)
	target_link_libraries(${TOOL_NAME} libfwk)
endforeach()
