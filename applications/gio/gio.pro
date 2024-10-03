QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

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
    remove.h \
    rename.h \
    version.h

TARGET = gio
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

include(../../qmake/liboxide.pri)
