QT -= gui

CONFIG += c++11
CONFIG += console
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

HEADERS +=

TARGET = fbinfo
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

include(../../qmake/liboxide.pri)
