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

LIBS += $$SDL_LIBS_DIR/SDL2main.lib
LIBS += $$SDL_LIBS_DIR/SDL2.lib

SOURCES += \
	../src/main.cpp


PLB_DUBUG {
	DEFINES+= DEBUG
} else {
}
