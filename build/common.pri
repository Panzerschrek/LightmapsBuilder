CONFIG -= app_bundle
CONFIG += windows
CONFIG -= qt
CONFIG += c++11

CONFIG( debug, debug|release ) {
	CONFIG+= PLB_DUBUG
}

INCLUDEPATH += ../../panzer_ogl_lib
INCLUDEPATH += ../src

PLB_DUBUG {
	DEFINES+= DEBUG
} else {
}
