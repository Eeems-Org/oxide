QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

TARGET = fret
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.fret.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

include(../../qmake/liboxide.pri)
