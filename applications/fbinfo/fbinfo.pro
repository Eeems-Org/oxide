QT -= gui

CONFIG += c++11
CONFIG += console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

HEADERS +=

TARGET = fbinfo
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

