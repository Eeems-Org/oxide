QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += qtquickcompiler

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

TARGET = decay
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

HEADERS += \
    controller.h

RESOURCES += \
    qml.qrc

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
