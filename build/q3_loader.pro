TEMPLATE= lib
CONFIG+= dll

include (common.pri)

Q3_SRC_DIR = ../../Quake-III-Arena

INCLUDEPATH += $$Q3_SRC_DIR

SOURCES += \
	../src/q3_bsp_loader.cpp \
	$$Q3_SRC_DIR/common/bspfile.c \
	$$Q3_SRC_DIR/common/cmdlib.c \
	$$Q3_SRC_DIR/common/scriplib.c \

HEADERS += \
	../src/formats.hpp \
	../src/q3_bsp_loader.hpp

DEFINES += PLB_DLL_BUILD
