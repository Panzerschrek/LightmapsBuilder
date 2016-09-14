TEMPLATE = app

include (common.pri)

OBJECTS_DIR = lightmaps_builder

SDL_INCLUDES_DIR = ../../SDL2-2.0.3/include
SDL_LIBS_DIR = ../../SDL2-2.0.3/lib/x86

DEVIL_INCLUDES_DIR = ../../DevIL/include
DEVIL_LIBS_DIR = ../../DevIL/lib

INCLUDEPATH += $$SDL_INCLUDES_DIR
INCLUDEPATH += $$DEVIL_INCLUDES_DIR

LIBS += $$SDL_LIBS_DIR/SDL2main.lib
LIBS += $$SDL_LIBS_DIR/SDL2.lib
LIBS += $$DEVIL_LIBS_DIR/DevIL.lib
LIBS += $$DEVIL_LIBS_DIR/ILU.lib
LIBS+= libopengl32

SOURCES += \
	../src/camera_controller.cpp \
	../src/curves.cpp \
	../src/lightmaps_builder.cpp \
	../src/lights_visualizer.cpp \
	../src/loaders_common.cpp \
	../src/main.cpp \
	../src/math_utils.cpp \
	../src/textures_manager.cpp \
	../src/tracer.cpp \
	../src/world_vertex_buffer.cpp \
	../../panzer_ogl_lib/polygon_buffer.cpp \
	../../panzer_ogl_lib/shaders_loading.cpp \
	../../panzer_ogl_lib/texture.cpp \
	../../panzer_ogl_lib/framebuffer.cpp \
	../../panzer_ogl_lib/func_addresses.cpp \
	../../panzer_ogl_lib/glsl_program.cpp \
	../../panzer_ogl_lib/matrix.cpp \
	../../panzer_ogl_lib/ogl_state_manager.cpp

HEADERS += \
	../src/camera_controller.hpp \
	../src/curves.hpp \
	../src/formats.hpp \
	../src/lightmaps_builder.hpp \
	../src/lights_visualizer.hpp \
	../src/loaders_common.hpp \
	../src/math_utils.hpp \
	../src/rasterizer.hpp \
	../src/textures_manager.hpp \
	../src/tracer.hpp \
	../src/world_vertex_buffer.hpp \
	../../panzer_ogl_lib/ogl_state_manager.hpp \
	../../panzer_ogl_lib/panzer_ogl_lib.hpp \
	../../panzer_ogl_lib/polygon_buffer.hpp \
	../../panzer_ogl_lib/shaders_loading.hpp \
	../../panzer_ogl_lib/texture.hpp \
	../../panzer_ogl_lib/vec.hpp \
	../../panzer_ogl_lib/framebuffer.hpp \
	../../panzer_ogl_lib/func_declarations.hpp \
	../../panzer_ogl_lib/glsl_program.hpp \
	../../panzer_ogl_lib/matrix.hpp \
	../../panzer_ogl_lib/bbox.hpp \
