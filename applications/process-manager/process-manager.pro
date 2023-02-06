QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qtquickcompiler
CONFIG += precompile_header

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp

RESOURCES += qml.qrc

TARGET = erode
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

icons.path = /opt/usr/share/icons/oxide/48x48/apps/
icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/erode.png
INSTALLS += icon

splash.path = /opt/usr/share/icons/oxide/702x702/splash/
splash.files += ../../assets/opt/usr/share/icons/oxide/702x702/splash/erode.png
INSTALLS += splash


HEADERS += \
    controller.h \
    taskitem.h \
    tasklist.h

PRECOMPILED_HEADER = \
    erode_stable.h

LIBS += -lsystemd

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
