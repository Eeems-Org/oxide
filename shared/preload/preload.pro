TARGET = oxide_preload
TEMPLATE = lib

include(../../qmake/common.pri)

CONFIG += hide_symbols

QT -= gui

HEADERS +=

SOURCES += main.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL

QMAKE_LFLAGS += -Wl,--version-script $$_PRO_FILE_PWD_/main.map

target.path += /opt/lib
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

DEFINES += SHIM_INPUT_FOR_PRELOAD
DEFINES += QT_MESSAGELOGCONTEXT

include(../../qmake/liboxide.pri)
