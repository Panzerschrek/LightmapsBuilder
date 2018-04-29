TEMPLATE= lib
CONFIG+= dll

include (common.pri)

Q2_SRC_DIR = ../../Quake-2-Tools

INCLUDEPATH += $$Q2_SRC_DIR

OBJECTS_DIR = q2_loader

SOURCES += \
	../src/math_utils.cpp \
	../src/loaders_common.cpp \
	../src/q2_bsp_loader.cpp \
	../panzer_ogl_lib/matrix.cpp \
	$$Q2_SRC_DIR/common/bspfile.c \
	$$Q2_SRC_DIR/common/cmdlib.c \
	$$Q2_SRC_DIR/common/scriplib.c \

HEADERS += \
	../src/formats.hpp \
	../src/loaders_common.hpp \
	../src/math_utils.hpp \
	../src/q3_bsp_loader.hpp \
	../panzer_ogl_lib/matrix.hpp \
	../panzer_ogl_lib/vec.hpp \

DEFINES += PLB_DLL_BUILD
