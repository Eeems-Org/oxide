QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

HEADERS +=

TARGET = codes.eeems.openwith
include(../../qmake/common.pri)
target.path = $$ROOT_INSTALL_PATH/share/oxide/libexec
INSTALLS += target

include(../../qmake/liboxide.pri)