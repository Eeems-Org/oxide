TARGET = oxide_qt_preload
TEMPLATE = lib

include(../../qmake/common.pri)

CONFIG += hide_symbols

QT = core

SOURCES += main.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL
LIBS += -lsystemd

target.path += /opt/lib
INSTALLS += target

DEFINES += QT_MESSAGELOGCONTEXT
