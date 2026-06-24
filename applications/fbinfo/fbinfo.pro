QT -= gui

CONFIG += c++11
CONFIG += console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

HEADERS +=

TARGET = fbinfo
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

