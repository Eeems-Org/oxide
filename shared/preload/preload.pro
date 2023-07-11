TARGET = oxide_preload
TEMPLATE = lib

include(../../qmake/common.pri)

CONFIG += hide_symbols

QT -= gui

SOURCES += main.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL

target.path += /opt/lib
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

DEFINES += SHIM_INPUT_FOR_PRELOAD

include(../../qmake/liboxide.pri)
