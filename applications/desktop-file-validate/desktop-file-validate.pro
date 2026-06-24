QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

HEADERS +=

TARGET = desktop-file-validate
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

include(../../qmake/liboxide.pri)
