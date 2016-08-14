TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++11


CONFIG( debug, debug|release ) {
	CONFIG+= PLB_DUBUG
}

SOURCES += \
	../src/main.cpp


PLB_DUBUG {
	DEFINES+= DEBUG
} else {
}
