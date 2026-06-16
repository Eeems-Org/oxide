TARGET = blight_client
TEMPLATE = lib

DEFINES += DISABLE_LTO
include(../../qmake/common.pri)

CONFIG += hide_symbols

QT =

HEADERS += \
    drm.h \
    fb.h \
    input.h \
    libc.h \
    qt.h \
    state.h \
    fopen.h

SOURCES += \
    drm.cpp \
    fb.cpp \
    input.cpp \
    libc.cpp \
    main.cpp \
    qt.cpp \
    state.cpp \
    fopen.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL
PKGCONFIG += libsystemd

QMAKE_LFLAGS += -Wl,--as-needed
QMAKE_LFLAGS +=  -Wl,--version-script $$_PRO_FILE_PWD_/main.map
QMAKE_CFLAGS += -fno-builtin
QMAKE_CXXFLAGS += -fno-builtin

target.path += /opt/lib
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

DEFINES += SHIM_INPUT_FOR_PRELOAD

include(../../qmake/libblight.pri)

DISTFILES += \
    main.map
