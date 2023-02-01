QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        common.cpp \
        main.cpp

HEADERS += \
    cat.h \
    common.h \
    copy.h \
    help.h \
    launch.h \
    mkdir.h \
    open.h \
    version.h

TARGET = gio
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
