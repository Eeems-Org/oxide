TARGET = oxide_sysfs_preload
TEMPLATE = lib

include(../../qmake/common.pri)

CONFIG += hide_symbols
CONFIG += c++17

QT = core
QT += network

SOURCES += main.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL
LIBS += -lsystemd

target.path += /opt/lib
INSTALLS += target

DEFINES += QT_MESSAGELOGCONTEXT
