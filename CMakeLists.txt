cmake_minimum_required(VERSION 2.8)

project( panzer_lightmaps_builder )

set( CMAKE_CXX_STANDARD 11 )

function( SetupCSources C_SOURCES )
	foreach( C_SOURCE ${C_SOURCES} )
		set_source_files_properties( ${C_SOURCE} COMPILE_FLAGS "-x c"  )
	endforeach()
endfunction()

#
# Q1 Loader
#

set( Q1_SRC_DIR ../Quake-Tools/qutils )

set( Q1_LOADER_SOURCES
	src/loaders_common.cpp
	src/math_utils.cpp
	src/q1_bsp_loader.cpp
	panzer_ogl_lib/matrix.cpp
	)

set( Q1_LOADER_SOURCES_C
	${Q1_SRC_DIR}/COMMON/BSPFILE.c
	${Q1_SRC_DIR}/COMMON/CMDLIB.c
	${Q1_SRC_DIR}/COMMON/SCRIPLIB.c
	${Q1_SRC_DIR}/LIGHT/ENTITIES.c
	)

add_library( q1_loader SHARED ${Q1_LOADER_SOURCES} ${Q1_LOADER_SOURCES_C} )
target_include_directories(
	q1_loader PRIVATE
	${Q1_SRC_DIR}
	${Q1_SRC_DIR}/COMMON
	panzer_ogl_lib )
target_compile_definitions( q1_loader PRIVATE PLB_DLL_BUILD )
SetupCSources( "${Q1_LOADER_SOURCES_C}" )
set_target_properties( q1_loader PROPERTIES PREFIX "" )

#
# Q2 Loader
#

set( Q2_SRC_DIR ../Quake-2-Tools )

set( Q2_LOADER_SOURCES
	src/math_utils.cpp
	src/loaders_common.cpp
	src/q2_bsp_loader.cpp
	panzer_ogl_lib/matrix.cpp
	)

set( Q2_LOADER_SOURCES_C
	${Q2_SRC_DIR}/common/bspfile.c
	${Q2_SRC_DIR}/common/cmdlib.c
	${Q2_SRC_DIR}/common/scriplib.c
	)

add_library( q2_loader SHARED ${Q2_LOADER_SOURCES} ${Q2_LOADER_SOURCES_C} )
target_include_directories(
	q2_loader PRIVATE
	${Q2_SRC_DIR}
	panzer_ogl_lib )
target_compile_definitions( q2_loader PRIVATE PLB_DLL_BUILD )
SetupCSources( "${Q2_LOADER_SOURCES_C}" )
set_target_properties( q2_loader PROPERTIES PREFIX "" )

#
# Q3 Loader
#

set( Q3_SRC_DIR ../Quake-III-Arena )

set( Q3_LOADER_SOURCES
	src/loaders_common.cpp
	src/q3_bsp_loader.cpp
	)

set( Q3_LOADER_SOURCES_C
	${Q3_SRC_DIR}/common/bspfile.c
	${Q3_SRC_DIR}/common/cmdlib.c
	${Q3_SRC_DIR}/common/scriplib.c
	)

add_library( q3_loader SHARED ${Q3_LOADER_SOURCES} ${Q3_LOADER_SOURCES_C} )
target_include_directories(
	q3_loader PRIVATE
	${Q3_SRC_DIR}
	panzer_ogl_lib )
target_compile_definitions( q3_loader PRIVATE PLB_DLL_BUILD )
SetupCSources( "${Q3_LOADER_SOURCES_C}" )
set_target_properties( q3_loader PROPERTIES PREFIX "" )

#
# HL Loader
#

set( HL_SRC_DIR ../halflife )

set( HL_LOADER_SOURCES
	src/math_utils.cpp
	src/loaders_common.cpp
	src/hl_bsp_loader.cpp
	panzer_ogl_lib/matrix.cpp
	)

set( HL_LOADER_SOURCES_C
	${HL_SRC_DIR}/utils/common/bspfile.c
	${HL_SRC_DIR}/utils/common/cmdlib.c
	${HL_SRC_DIR}/utils/common/scriplib.c
	)

add_library( hl_loader SHARED ${HL_LOADER_SOURCES} ${HL_LOADER_SOURCES_C} )
target_include_directories(
	hl_loader PRIVATE
	${HL_SRC_DIR}
	${HL_SRC_DIR}/utils/common
	panzer_ogl_lib )
target_compile_definitions( hl_loader PRIVATE PLB_DLL_BUILD )
SetupCSources( "${HL_LOADER_SOURCES_C}" )
set_target_properties( hl_loader PROPERTIES PREFIX "" )

#
# Lightmaps builder
#

set( LIGHTMAPS_BUILDER_SOURCES
	src/camera_controller.cpp
	src/curves.cpp
	src/lightmaps_builder.cpp
	src/lights_visualizer.cpp
	src/loaders_common.cpp
	src/main.cpp
	src/math_utils.cpp
	src/textures_manager.cpp
	src/tracer.cpp
	src/world_vertex_buffer.cpp
	panzer_ogl_lib/polygon_buffer.cpp
	panzer_ogl_lib/shaders_loading.cpp
	panzer_ogl_lib/texture.cpp
	panzer_ogl_lib/framebuffer.cpp
	panzer_ogl_lib/func_addresses.cpp
	panzer_ogl_lib/glsl_program.cpp
	panzer_ogl_lib/matrix.cpp
	panzer_ogl_lib/ogl_state_manager.cpp
	)

set( SDL_BASE_DIR ../SDL2-2.0.3 )
set( SDL_INCLUDES_DIR ${SDL_BASE_DIR}/include )
set( SDL_LIBS_DIR ${SDL_BASE_DIR}/lib/x86 )
get_filename_component( SDL_LIBS_DIR_ABSOLUTE ${SDL_LIBS_DIR} ABSOLUTE )

set( DEVIL_BASE_DIR ../DevIL/ )
set( DEVIL_INCLUDES_DIR ${DEVIL_BASE_DIR}/include )
set( DEVIL_LIBS_DIR ${DEVIL_BASE_DIR}/lib/ )
get_filename_component( DEVIL_LIBS_DIR_ABSOLUTE ${DEVIL_LIBS_DIR} ABSOLUTE )

add_executable( lightmaps_builder ${LIGHTMAPS_BUILDER_SOURCES} )
target_include_directories( lightmaps_builder PRIVATE panzer_ogl_lib )
target_include_directories( lightmaps_builder PRIVATE ${SDL_INCLUDES_DIR} )
target_include_directories( lightmaps_builder PRIVATE ${DEVIL_INCLUDES_DIR} )
target_link_libraries( lightmaps_builder ${SDL_LIBS_DIR_ABSOLUTE}/SDL2main.lib )
target_link_libraries( lightmaps_builder ${SDL_LIBS_DIR_ABSOLUTE}/SDL2.lib )
target_link_libraries( lightmaps_builder ${DEVIL_LIBS_DIR_ABSOLUTE}/DevIL.lib )
target_link_libraries( lightmaps_builder ${DEVIL_LIBS_DIR_ABSOLUTE}/ILU.lib )
target_link_libraries( lightmaps_builder opengl32 )
