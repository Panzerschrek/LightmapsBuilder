TEMPLATE= lib
CONFIG+= dll

include (common.pri)

Q3_SRC_DIR = ../../Quake-2-Tools

INCLUDEPATH += $$Q3_SRC_DIR

SOURCES += \
	../src/math_utils.cpp \
	../src/q2_bsp_loader.cpp \
	$$Q3_SRC_DIR/common/bspfile.c \
	$$Q3_SRC_DIR/common/cmdlib.c \
	$$Q3_SRC_DIR/common/scriplib.c \

HEADERS += \
	../src/formats.hpp \
	../src/math_utils.hpp \
	../src/q3_bsp_loader.hpp

DEFINES += PLB_DLL_BUILD Q2_LOADER
