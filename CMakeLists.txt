# CMakeLists.txt for libfwk library

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =================================================================================================
# region                                    Options
# =================================================================================================

option(FWK_BUILD_TESTS "Enable building of tests" ON)
option(FWK_BUILD_TOOLS "Enable building of tools" ON)
option(FWK_UNITY_BUILD "Enable unity builds for faster compilation" OFF)

option(FWK_GEOM "Enable geometry module" ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/imgui.h")
option(FWK_IMGUI "Enable ImGui support" ON)
else()
option(FWK_IMGUI "Enable ImGui support" OFF)
endif()

# Export to cache so other included files can see the choices
set(FWK_BUILD_TESTS ${FWK_BUILD_TESTS} CACHE BOOL "Enable building of tests" FORCE)
set(FWK_BUILD_TOOLS ${FWK_BUILD_TOOLS} CACHE BOOL "Enable building of tools" FORCE)

message(STATUS "FWK: Build tests = ${FWK_BUILD_TESTS}")
message(STATUS "FWK: Build tools = ${FWK_BUILD_TOOLS}")
message(STATUS "FWK: Unity build = ${FWK_UNITY_BUILD}")
message(STATUS "FWK: Enable geometry module = ${FWK_GEOM}")
message(STATUS "FWK: Enable ImGui support = ${FWK_IMGUI}")

# =================================================================================================
# region                   Compiler configuration & default compiler flags
# =================================================================================================

if(FWK_UNITY_BUILD)
	set(CMAKE_UNITY_BUILD ON)
endif()

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	set(CMAKE_GENERATOR_TOOLSET "ClangCL")
endif()

# Set custom configuration types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Develop" CACHE STRING "" FORCE)

# Configuration-specific compiler flags
if (CMAKE_GENERATOR MATCHES "Visual Studio" OR TRUE)
	# MSVC flags
	set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /MDd" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_DEVELOP "/Zi /O2 /MD" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /MD" CACHE STRING "" FORCE)

	set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_DEVELOP ${CMAKE_CXX_FLAGS_DEVELOP} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} CACHE STRING "" FORCE)
else()
	# GCC/Clang flags
	set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_DEVELOP "-g -O2" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE STRING "" FORCE)

	set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_DEVELOP ${CMAKE_CXX_FLAGS_DEVELOP} CACHE STRING "" FORCE)
	set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} CACHE STRING "" FORCE)
endif()

add_compile_definitions(
	$<$<CONFIG:Debug>:FWK_DEBUG>
	$<$<CONFIG:Develop>:FWK_DEVELOP>
	$<$<CONFIG:Release>:FWK_RELEASE>
)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Develop" CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "Develop")
endif()


# =================================================================================================
# region 				                 Utility functions
# =================================================================================================

function(fwk_add_program GROUP NAME)
	# Optional argument: SOURCES
	set(SOURCES "${ARGV2}")
	if(NOT SOURCES)
		set(SOURCES "${GROUP}/${NAME}")
	endif()

	set(_FILES "")
	foreach(SOURCE IN LISTS SOURCES)
		list(APPEND _FILES "src/${SOURCE}.cpp")
	endforeach()

	set(TARGET_NAME "${GROUP}_${NAME}")
	add_executable(${TARGET_NAME} ${_FILES})
	target_link_libraries(${TARGET_NAME} PRIVATE libfwk)
	set_target_properties(${TARGET_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/${GROUP}")
	set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_NAME "${NAME}")
endfunction()

function(fwk_add_module NAME)
	set(_FILES "")
	foreach(SOURCE ${SRC_${NAME}})
		list(APPEND _FILES "src/${SOURCE}.cpp")
	endforeach()

	string(REGEX REPLACE "_.*" "" GROUP_NAME ${NAME}) 
	source_group("${GROUP_NAME}" FILES ${_FILES})

	set(_LIBFWK_SOURCES "${LIBFWK_SOURCES}")
	list(APPEND _LIBFWK_SOURCES ${_FILES})
	set(LIBFWK_SOURCES "${_LIBFWK_SOURCES}" PARENT_SCOPE)

	set_source_files_properties(${_FILES} PROPERTIES UNITY_GROUP "${NAME}")
endfunction()


# =================================================================================================
# region 				              libfwk project definition
# =================================================================================================

project(libfwk)

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
	gfx/colored_triangle gfx/drawing gfx/dynamic_mesh
	gfx/fpp_camera gfx/image gfx/image_tga gfx/investigate gfx/investigator2 gfx/investigator3
	gfx/matrix_stack gfx/orbiting_camera gfx/ortho_camera gfx/plane_camera gfx/shader_compiler
	gfx/shader_debug gfx/shader_defs gfx/shader_reflection)

set(SRC_gfx_font
    gfx/font_factory gfx/font_finder gfx/font)

set(SRC_gfx_mesh
	gfx/animated_model gfx/converter gfx/mesh_buffers gfx/mesh_constructor gfx/mesh gfx/mesh_indices
	gfx/model_anim gfx/model gfx/model_node gfx/pose)

set(SRC_vulkan
	vulkan/vulkan_internal vulkan/vulkan_buffer vulkan/vulkan_command_queue
	vulkan/vulkan_descriptor_manager vulkan/vulkan_device vulkan/vulkan_framebuffer
	vulkan/vulkan_image vulkan/vulkan_instance vulkan/vulkan_memory_manager vulkan/vulkan_pipeline
	vulkan/vulkan_shader vulkan/vulkan_storage vulkan/vulkan_swap_chain vulkan/vulkan_window
	vulkan/vulkan_ray_tracing vulkan/vulkan_render_pass vulkan_base)

set(SRC_gfx_stbi
	gfx/image_stbi gfx/image_stbir)

set(SRC_audio
	audio/al_device audio/sound audio/ogg_stream)

set(SRC_perf
	perf/perf_base perf/exec_tree perf/manager perf/thread_context)

set(MODULES base sys math gfx gfx_font gfx_mesh vulkan gfx_stbi audio perf)

if(FWK_GEOM)
	set(SRC_geom geom_base geom/contour geom/regular_grid geom/segment_grid geom/procgen)
	set(SRC_geom_graph geom/element_ref geom/graph geom/geom_graph)
	set(SRC_geom_voronoi geom/voronoi geom/wide_int geom/voronoi_constructor geom/delaunay)
	list(APPEND MODULES geom geom_graph geom_voronoi)
endif()

if(FWK_IMGUI)
	set(SRC_gui_imgui1 gui/imgui_base)
	set(SRC_gui_imgui2 gui/imgui_draw)
	set(SRC_gui_imgui3 gui/imgui_widgets gui/imgui_tables)
	set(SRC_gui_imgui4 gui/imgui_demo)
	set(SRC_gui gui/gui gui/popups gui/widgets perf/analyzer)
	list(APPEND MODULES gui_imgui1 gui_imgui2 gui_imgui3 gui_imgui4 gui)
endif()

foreach(MODULE ${MODULES})
	fwk_add_module(${MODULE})
endforeach()

add_library(libfwk ${LIBFWK_SOURCES})

if(FWK_BUILD_TESTS)
	if(FWK_GEOM)
		fwk_add_program(tests geom)
	endif()
	fwk_add_program(tests hash_map_perf)
	fwk_add_program(tests math)
	fwk_add_program(tests models)
	fwk_add_program(tests stuff)
	fwk_add_program(tests variant_perf)
	fwk_add_program(tests vector_perf)
	fwk_add_program(tests window)
endif()

if(FWK_BUILD_TOOLS)
	fwk_add_program(tools model_convert)
	fwk_add_program(tools model_viewer)
endif()

include_directories(src/ extern/imgui/)

target_include_directories(libfwk
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_directories(libfwk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib)

if(WIN32)
	target_link_libraries(libfwk
		SDL3 dbghelp winmm version setupapi dwrite
		shaderc-combined-$<IF:$<CONFIG:Release>,md,mdd>)
else()
	target_link_libraries(libfwk SDL3 shaderc-combined)
endif()

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	target_sources(libfwk PRIVATE libfwk.natstepfilter)
	target_sources(libfwk PRIVATE libfwk.natvis)
endif()

if(FWK_UNITY_BUILD)
	set_target_properties(libfwk PROPERTIES UNITY_BUILD ON)
	set_target_properties(libfwk PROPERTIES UNITY_BUILD_MODE GROUP)
endif()

# This is required for clang __int128 support on Windows
if ((CMAKE_GENERATOR MATCHES "Visual Studio") AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR TRUE)
    string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" CLANG_MAJOR_VERSION ${CMAKE_CXX_COMPILER_VERSION})
	get_filename_component(CLANG_MAIN_DIR_PATH "${CMAKE_CXX_COMPILER}/../.." ABSOLUTE)
	set(CLANG_RT_LIB_PATH "${CLANG_MAIN_DIR_PATH}/lib/clang/${CLANG_MAJOR_VERSION}/lib/windows/")
	target_link_libraries(libfwk ${CLANG_RT_LIB_PATH}/clang_rt.builtins-x86_64.lib)
endif()

if ((CMAKE_GENERATOR MATCHES "Visual Studio") AND NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
	message(FATAL_ERROR "Visual Studio builds require ClangCL toolset.\n"
			"You can enable it with: set(CMAKE_GENERATOR_TOOLSET \"ClangCL\")")
endif()
