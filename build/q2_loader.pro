TEMPLATE= lib
CONFIG+= dll

include (common.pri)

Q2_SRC_DIR = ../../Quake-2-Tools

INCLUDEPATH += $$Q3_SRC_DIR

SOURCES += \
	../src/math_utils.cpp \
	../src/q2_bsp_loader.cpp \
	../../panzer_ogl_lib/matrix.cpp \
	$$Q2_SRC_DIR/common/bspfile.c \
	$$Q2_SRC_DIR/common/cmdlib.c \
	$$Q2_SRC_DIR/common/scriplib.c \

HEADERS += \
	../src/formats.hpp \
	../src/math_utils.hpp \
	../src/q3_bsp_loader.hpp \
	../../panzer_ogl_lib/matrix.hpp \
	../../panzer_ogl_lib/vec.hpp \

DEFINES += PLB_DLL_BUILD Q2_LOADER
