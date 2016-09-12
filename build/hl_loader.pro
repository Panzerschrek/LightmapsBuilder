TEMPLATE= lib
CONFIG+= dll

include (common.pri)

HL_SRC_DIR = ../../halflife

INCLUDEPATH += $$HL_SRC_DIR
INCLUDEPATH += $$HL_SRC_DIR/utils/common

OBJECTS_DIR = hl_loader

SOURCES += \
	../src/math_utils.cpp \
	../src/loaders_common.cpp \
	../src/hl_bsp_loader.cpp \
	../../panzer_ogl_lib/matrix.cpp \
	$$HL_SRC_DIR/utils/common/cmdlib.c \
	$$HL_SRC_DIR/utils/common/bspfile.c \
	$$HL_SRC_DIR/utils/common/scriplib.c \

HEADERS += \
	../src/formats.hpp \
	../src/loaders_common.hpp \
	../src/math_utils.hpp \
	../src/q3_bsp_loader.hpp \
	../../panzer_ogl_lib/matrix.hpp \
	../../panzer_ogl_lib/vec.hpp \

DEFINES += PLB_DLL_BUILD
