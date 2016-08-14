TEMPLATE = app
CONFIG -= app_bundle
CONFIG += windows
CONFIG -= qt
CONFIG += c++11

CONFIG( debug, debug|release ) {
	CONFIG+= PLB_DUBUG
}

SDL_INCLUDES_DIR = ../../SDL2-2.0.3/include
SDL_LIBS_DIR = ../../SDL2-2.0.3/lib/x86

INCLUDEPATH += $$SDL_INCLUDES_DIR
INCLUDEPATH += ../../panzer_ogl_lib
INCLUDEPATH += ../src

LIBS += $$SDL_LIBS_DIR/SDL2main.lib
LIBS += $$SDL_LIBS_DIR/SDL2.lib
LIBS+= libopengl32

PLB_DUBUG {
	DEFINES+= DEBUG
} else {
}

SOURCES += \
	../src/camera_controller.cpp \
	../src/main.cpp \
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
	../src/formats.hpp \
	../../panzer_ogl_lib/ogl_state_manager.hpp \
	../../panzer_ogl_lib/panzer_ogl_lib.hpp \
	../../panzer_ogl_lib/polygon_buffer.hpp \
	../../panzer_ogl_lib/shaders_loading.hpp \
	../../panzer_ogl_lib/texture.hpp \
	../../panzer_ogl_lib/vec.hpp \
	../../panzer_ogl_lib/framebuffer.hpp \
	../../panzer_ogl_lib/func_declarations.hpp \
	../../panzer_ogl_lib/glsl_program.hpp \
	../../panzer_ogl_lib/matrix.hpp
