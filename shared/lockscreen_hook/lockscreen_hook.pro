TARGET = lockscreen_hook
TEMPLATE = lib

DEFINES += DISABLE_LTO
DEFINES += DISABLE_UNWIND
include(../../qmake/common.pri)

CONFIG += hide_symbols

QT += core

HEADERS += \
    receiver.h

SOURCES += \
    main.cpp \
    receiver.cpp

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL

QMAKE_LFLAGS += -Wl,--as-needed
QMAKE_CFLAGS += -fno-builtin
QMAKE_CXXFLAGS += -fno-builtin

target.path += /usr/lib
INSTALLS += target

DISTFILES += \
    ../../assets/usr/lib/systemd/system/xochitl.service.d/lockscreen_hook.conf
