# Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
# This file is part of libfwk. See license.txt for details.

# TODO: paranoid mode
# TODO: PCH
# TODO: look for more stuff worth restoring from makefiles

cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_GENERATOR MATCHES "Visual Studio")
	set(CMAKE_GENERATOR_TOOLSET "ClangCL")
endif()

# =================================================================================================
# region                                    Options
# =================================================================================================

option(FWK_BUILD_TESTS "Enable building of tests" ON)
option(FWK_BUILD_TOOLS "Enable building of tools" ON)
option(FWK_UNITY_BUILD "Enable unity builds for faster compilation" OFF)

option(FWK_GEOM "Enable geometry module" ON)

set(FWK_DEPENDENCIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies" CACHE PATH
	"Directory containing libfwk dependencies")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/imgui.h")
option(FWK_IMGUI "Enable ImGui support" ON)
else()
option(FWK_IMGUI "Enable ImGui support" OFF)
endif()

option(FWK_IMGUI_DEMO "Enable ImGui demo window in ImGui-based apps" OFF)

# Export to cache so other included files can see the choices
set(FWK_BUILD_TESTS ${FWK_BUILD_TESTS} CACHE BOOL "Enable building of tests" FORCE)
set(FWK_BUILD_TOOLS ${FWK_BUILD_TOOLS} CACHE BOOL "Enable building of tools" FORCE)

message(STATUS "FWK: Build tests = ${FWK_BUILD_TESTS}")
message(STATUS "FWK: Build tools = ${FWK_BUILD_TOOLS}")
message(STATUS "FWK: Unity build = ${FWK_UNITY_BUILD}")
message(STATUS "FWK: Enable geometry module = ${FWK_GEOM}")
message(STATUS "FWK: Enable ImGui support = ${FWK_IMGUI}")
message(STATUS "FWK: Dependencies dir = ${FWK_DEPENDENCIES_DIR}")

# =================================================================================================
# region                   Compiler configuration & default compiler flags
# =================================================================================================

if(FWK_UNITY_BUILD)
	set(CMAKE_UNITY_BUILD ON)
endif()


# Set custom configuration types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Develop" CACHE STRING "" FORCE)

# Configuration-specific compiler flags
if (WIN32)
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
	FWK_DWARF_DISABLED
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
	set(_SOURCES "${GROUP}/${NAME}.cpp")
	list(TRANSFORM _SOURCES PREPEND "src/")

	set(TARGET_NAME "${GROUP}_${NAME}")
	add_executable(${TARGET_NAME} ${_SOURCES})
	target_link_libraries(${TARGET_NAME} PRIVATE libfwk)
	set_target_properties(${TARGET_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/${GROUP}")
	set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_NAME "${NAME}")
endfunction()

function(fwk_add_module NAME)
	set(_SOURCES ${SRC_${NAME}})
	list(TRANSFORM _SOURCES PREPEND "src/")
	set(_HEADERS ${HDR_${NAME}})
	list(TRANSFORM _HEADERS PREPEND "include/fwk/")

	string(REGEX REPLACE "_.*" "" GROUP_NAME ${NAME}) 
	source_group("${GROUP_NAME}" FILES ${_SOURCES})
	source_group("${GROUP_NAME}" FILES ${_HEADERS})
	target_sources(libfwk PRIVATE ${_SOURCES})
	target_sources(libfwk PUBLIC FILE_SET HEADERS BASE_DIRS include FILES ${_HEADERS})
	set_source_files_properties(${_SOURCES} PROPERTIES UNITY_GROUP "${NAME}")
endfunction()


# =================================================================================================
# region 				              libfwk project definition
# =================================================================================================

project(libfwk)

set(PCH_SOURCE "src/fwk_pch.h")
if(WIN32)
	# MSVC-specific PCH handling if needed
elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# GCC/Clang PCH handling
endif()

set(HDR_base
	algorithm.h
	any.h
	any_config.h
	array.h
	base_vector.h
	bit_vector.h
	dynamic.h
	enum.h
	enum_flags.h
	enum_map.h
	format.h
	fwd_member.h
	hash_map.h
	hash_map_stats.h
	hash_map_storage.h
	hash_set.h
	heap.h
	index_range.h
	light_tuple.h
	list_node.h
	logger.h
	maybe.h
	maybe_ref.h
	meta_base.h
	meta/iterator.h
	meta/operator.h
	meta/range.h
	parse.h
	pod_vector.h
	slab_allocator.h
	span.h
	sparse_span.h
	sparse_vector.h
	static_vector.h
	str.h
	tag_id.h
	type_info.h
	type_info_gen.h
	variant.h
	vector.h
	vector_map.h
)

set(SRC_base
	any.cpp
	any_config.cpp
	base_vector.cpp
	bit_vector.cpp
	enum.cpp
	format.cpp
	fwk_pch.h
	hash_map_stats.cpp
	logger.cpp
	parse.cpp
	slab_allocator.cpp
	str.cpp
	type_info.cpp
)

set(HDR_sys
	io/file_stream.h
	io/file_system.h
	io/gzip_stream.h
	io/memory_stream.h
	io/package_file.h
	io/stream.h
	io/url_fetch.h
	io/xml.h
	sys/assert.h
	sys/assert_impl.h
	sys/backtrace.h
	sys/error.h
	sys/exception.h
	sys/expected.h
	sys/input.h
	sys/memory.h
	sys/on_fail.h
	sys/platform.h
	sys/thread.h
	sys_base.h
)

set(SRC_sys
	io/file_stream.cpp
	io/file_system.cpp
	io/gzip_stream.cpp
	io/memory_stream.cpp
	io/package_file.cpp
	io/stream.cpp
	io/url_fetch.cpp
	io/xml.cpp
	sys/assert.cpp
	sys/assert_impl.cpp
	sys/backtrace.cpp
	sys/error.cpp
	sys/exception.cpp
	sys/expected.cpp
	sys/input.cpp
	sys/memory.cpp
	sys/on_fail.cpp
	sys/thread.cpp
	sys_base.cpp
)

set(HDR_math
	math/affine_trans.h
	math/axis_angle.h
	math/box.h
	math/box_iter.h
	math/constants.h
	math/cylinder.h
	math/direction.h
	math/dual.h
	math/frustum.h
	math/gcd.h
	math/hash.h
	math/interpolation.h
	math/interval.h
	math/isect_param.h
	math/line.h
	math/matrix3.h
	math/matrix4.h
	math/obox.h
	math/plane.h
	math/projection.h
	math/qint.h
	math/quat.h
	math/random.h
	math/rational.h
	math/rational_angle.h
	math/ray.h
	math/rotation.h
	math/sat_test.h
	math/segment.h
	math/tetrahedron.h
	math/triangle.h
	math_base.h
)

set(SRC_math
	math/affine_trans.cpp
	math/base.cpp
	math/box.cpp
	math/cylinder.cpp
	math/frustum.cpp
	math/gcd.cpp
	math/line.cpp
	math/matrix3.cpp
	math/matrix4.cpp
	math/matrix4_transform.cpp
	math/obox.cpp
	math/plane.cpp
	math/projection.cpp
	math/qint.cpp
	math/quat.cpp
	math/random.cpp
	math/rational.cpp
	math/rational_angle.cpp
	math/ray.cpp
	math/rotation.cpp
	math/segment.cpp
	math/tetrahedron.cpp
	math/triangle.cpp
)

set(HDR_gfx
	gfx/animated_model.h
	gfx/camera_control.h
	gfx/camera_variant.h
	gfx/camera.h
	gfx/canvas_2d.h
	gfx/canvas_3d.h
	gfx/color.h
	gfx/colored_quad.h
	gfx/colored_triangle.h
	gfx/converter.h
	gfx/converter.h
	gfx/drawing.h
	gfx/dynamic_mesh.h
	gfx/font_factory.h
	gfx/font_finder.h
	gfx/font.h
	gfx/fpp_camera.h
	gfx/image_view.h
	gfx/image.h
	gfx/investigate.h
	gfx/investigator2.h
	gfx/investigator3.h
	gfx/matrix_stack.h
	gfx/mesh_buffers.h
	gfx/mesh_indices.h
	gfx/mesh.h
	gfx/model_anim.h
	gfx/model_node.h
	gfx/model.h
	gfx/orbiting_camera.h
	gfx/ortho_camera.h
	gfx/plane_camera.h
	gfx/pose.h
	gfx/shader_compiler.h
	gfx/shader_debug.h
	gfx/shader_defs.h
	gfx/shader_reflection.h
)

set(SRC_gfx
	gfx/camera.cpp
	gfx/camera_control.cpp
	gfx/canvas_2d.cpp
	gfx/canvas_3d.cpp
	gfx/color.cpp
	gfx/colored_quad.cpp
	gfx/colored_triangle.cpp
	gfx/drawing.cpp
	gfx/dynamic_mesh.cpp
	gfx/fpp_camera.cpp
	gfx/image.cpp
	gfx/image_tga.cpp
	gfx/investigate.cpp
	gfx/investigator2.cpp
	gfx/investigator3.cpp
	gfx/matrix_stack.cpp
	gfx/orbiting_camera.cpp
	gfx/ortho_camera.cpp
	gfx/plane_camera.cpp
	gfx/shader_compiler.cpp
	gfx/shader_debug.cpp
	gfx/shader_defs.cpp
	gfx/shader_reflection.cpp
)

set(SRC_gfx_font
	gfx/font.cpp
	gfx/font_factory.cpp
	gfx/font_finder.cpp
)

set(SRC_gfx_mesh
	gfx/animated_model.cpp
	gfx/converter.cpp
	gfx/mesh.cpp
	gfx/mesh_buffers.cpp
	gfx/mesh_constructor.cpp
	gfx/mesh_indices.cpp
	gfx/model.cpp
	gfx/model_anim.cpp
	gfx/model_node.cpp
	gfx/pose.cpp
)

set(SRC_gfx_stbi
	gfx/image_stbi.cpp
	gfx/image_stbir.cpp
)

set(HDR_vulkan
	vulkan/vulkan_buffer.h
	vulkan/vulkan_buffer_span.h
	vulkan/vulkan_command_queue.h
	vulkan/vulkan_descriptor_manager.h
	vulkan/vulkan_device.h
	vulkan/vulkan_framebuffer.h
	vulkan/vulkan_image.h
	vulkan/vulkan_instance.h
	vulkan/vulkan_internal.h
	vulkan/vulkan_memory_manager.h
	vulkan/vulkan_pipeline.h
	vulkan/vulkan_ray_tracing.h
	vulkan/vulkan_render_pass.h
	vulkan/vulkan_shader.h
	vulkan/vulkan_storage.h
	vulkan/vulkan_swap_chain.h
	vulkan/vulkan_type_list.h
	vulkan/vulkan_window.h
	vulkan_base.h
)

set(SRC_vulkan
	# It has to be first for unity builds: it includes volk.c
	vulkan/vulkan_internal.cpp

	vulkan/vulkan_buffer.cpp
	vulkan/vulkan_command_queue.cpp
	vulkan/vulkan_descriptor_manager.cpp
	vulkan/vulkan_device.cpp
	vulkan/vulkan_framebuffer.cpp
	vulkan/vulkan_image.cpp
	vulkan/vulkan_instance.cpp
	vulkan/vulkan_memory_manager.cpp
	vulkan/vulkan_pipeline.cpp
	vulkan/vulkan_ray_tracing.cpp
	vulkan/vulkan_render_pass.cpp
	vulkan/vulkan_shader.cpp
	vulkan/vulkan_storage.cpp
	vulkan/vulkan_swap_chain.cpp
	vulkan/vulkan_window.cpp
	vulkan_base.cpp
)

set(HDR_audio
	audio/al_device.h
	audio/ogg_stream.h
	audio/sound.h
	audio_base.h
)

set(SRC_audio
	audio/al_device.cpp
	audio/ogg_stream.cpp
	audio/sound.cpp
)

set(HDR_perf
	perf/analyzer.h
	perf/exec_tree.h
	perf/manager.h
	perf/thread_context.h
	perf_base.h
)

set(SRC_perf
	perf/exec_tree.cpp
	perf/manager.cpp
	perf/perf_base.cpp
	perf/thread_context.cpp
)

set(HDR_geom
	geom/contour.h
	geom/contour_funcs.h
	geom/delaunay.h
	geom/element_ref.h
	geom/geom_graph.h
	geom/graph.h
	geom/procgen.h
	geom/regular_grid.h
	geom/segment_grid.h
	geom/voronoi.h
	geom/wide_int.h
)

set(SRC_geom
	geom/contour.cpp
	geom/procgen.cpp
	geom/regular_grid.cpp
	geom/segment_grid.cpp
	geom_base.cpp
)
set(SRC_geom_graph
	geom/element_ref.cpp
	geom/geom_graph.cpp
	geom/graph.cpp
)
set(SRC_geom_voronoi
	geom/delaunay.cpp
	geom/voronoi.cpp
	geom/voronoi_constructor.cpp
	geom/wide_int.cpp
)

set(SRC_gui_imgui1 ../extern/imgui/imgui.cpp)
set(SRC_gui_imgui2 ../extern/imgui/imgui_draw.cpp)
set(SRC_gui_imgui3 ../extern/imgui/imgui_widgets.cpp ../extern/imgui/imgui_tables.cpp)
set(SRC_gui_imgui_demo ../extern/imgui/imgui_demo.cpp)
set(SRC_gui
	gui/gui.cpp
	gui/popups.cpp
	gui/widgets.cpp
	perf/analyzer.cpp
)

set(MODULES base sys math gfx gfx_font gfx_mesh vulkan gfx_stbi audio perf)

if(FWK_GEOM)
	list(APPEND MODULES geom geom_graph geom_voronoi)
endif()

if(FWK_IMGUI)
	list(APPEND MODULES gui_imgui1 gui_imgui2 gui_imgui3 gui)
	if(FWK_IMGUI_DEMO)
		list(APPEND MODULES gui_imgui_demo)
	endif()
endif()

add_library(libfwk STATIC)

foreach(MODULE ${MODULES})
	fwk_add_module(${MODULE})
endforeach()


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
  PUBLIC  ${FWK_DEPENDENCIES_DIR}/include
  PUBLIC  ${FWK_DEPENDENCIES_DIR}/include/freetype2
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_directories(libfwk
  PUBLIC ${FWK_DEPENDENCIES_DIR}/lib)


if(WIN32)
	target_link_libraries(libfwk
		SDL3 dbghelp winmm version setupapi dwrite Imm32
		freetype zlib
		shaderc-combined-$<IF:$<CONFIG:Release>,md,mdd>)
else()
	target_link_libraries(libfwk SDL3 shaderc-combined freetype z)
	set_target_properties(libfwk PROPERTIES  OUTPUT_NAME "fwk")
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
if (WIN32)
    string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" CLANG_MAJOR_VERSION ${CMAKE_CXX_COMPILER_VERSION})
	get_filename_component(CLANG_MAIN_DIR_PATH "${CMAKE_CXX_COMPILER}/../.." ABSOLUTE)
	set(CLANG_RT_LIB_PATH "${CLANG_MAIN_DIR_PATH}/lib/clang/${CLANG_MAJOR_VERSION}/lib/windows/")
	target_link_libraries(libfwk ${CLANG_RT_LIB_PATH}/clang_rt.builtins-x86_64.lib)
endif()

if ((CMAKE_GENERATOR MATCHES "Visual Studio") AND NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
	message(FATAL_ERROR "Visual Studio builds require ClangCL toolset.\n"
			"You can enable it with: set(CMAKE_GENERATOR_TOOLSET \"ClangCL\")")
endif()
