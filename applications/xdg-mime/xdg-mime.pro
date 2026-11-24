QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
        common.cpp \
        main.cpp

HEADERS += \
    common.h \
    query.h \
    default.h

TARGET = xdg-mime
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

include(../../qmake/liboxide.pri)