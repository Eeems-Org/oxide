QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

HEADERS +=

TARGET = xdg-icon-resource
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

include(../../qmake/liboxide.pri)
