TARGET = blight_client
TEMPLATE = lib

include(../../qmake/common.pri)

CONFIG += hide_symbols

QT =

HEADERS += \
    input.h

SOURCES += main.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL
LIBS += -lsystemd

QMAKE_LFLAGS += -Wl,--version-script $$_PRO_FILE_PWD_/main.map

target.path += /opt/lib
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

DEFINES += SHIM_INPUT_FOR_PRELOAD

include(../../qmake/libblight.pri)
