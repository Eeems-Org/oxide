QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
        ../desktop-file-edit/edit.cpp \
        main.cpp

HEADERS += \
    ../desktop-file-edit/edit.h

TARGET = desktop-file-install
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

include(../../qmake/liboxide.pri)
