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

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.decay.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

HEADERS += \
    controller.h

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
