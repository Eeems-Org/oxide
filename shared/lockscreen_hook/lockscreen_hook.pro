!linux-oe-g++{
    error("epaper is only supported on the reMarkable")
}
TARGET = lockscreen_hook
TEMPLATE = lib

DEFINES += DISABLE_LTO
include(../../qmake/common.pri)

CONFIG += hide_symbols

QT += core

HEADERS += \
    receiver.h

SOURCES += \
    main.cpp \
    receiver.cpp

service.files = lockscreen_hook.conf
service.path = /etc/systemd/system/xochitl.service.d/
INSTALLS += service

LIBS += -lrt -ldl -Wl,--exclude-libs,ALL

QMAKE_LFLAGS += -Wl,--as-needed
QMAKE_CFLAGS += -fno-builtin
QMAKE_CXXFLAGS += -fno-builtin

target.path += /usr/lib
INSTALLS += target

client.files = lockscreen-hook
client.path = /usr/libexec/
INSTALLS += client

DISTFILES += \
    lockscreen_hook.conf \
    lockscreen-hook
