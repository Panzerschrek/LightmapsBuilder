TEMPLATE= lib
CONFIG+= dll

include (common.pri)

Q1_SRC_DIR = ../../Quake-Tools/qutils

INCLUDEPATH += $$Q1_SRC_DIR
INCLUDEPATH += $$Q1_SRC_DIR/COMMON

SOURCES += \
	../src/math_utils.cpp \
	../src/q1_bsp_loader.cpp \
	../../panzer_ogl_lib/matrix.cpp \
	$$Q1_SRC_DIR/COMMON/BSPFILE.c \
	$$Q1_SRC_DIR/COMMON/CMDLIB.c \
	$$Q1_SRC_DIR/COMMON/SCRIPLIB.c \
	$$Q1_SRC_DIR/LIGHT/ENTITIES.c \

HEADERS += \
	../src/formats.hpp \
	../src/math_utils.hpp \
	../src/q3_bsp_loader.hpp \
	../../panzer_ogl_lib/matrix.hpp \
	../../panzer_ogl_lib/vec.hpp \

DEFINES += PLB_DLL_BUILD
