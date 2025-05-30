QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qtquickcompiler
CONFIG += precompile_header# disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp

RESOURCES += qml.qrc

TARGET = erode
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.erode.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

icons.path = /opt/usr/share/icons/oxide/48x48/apps/
icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/erode.png
INSTALLS += icons

HEADERS += \
    controller.h \
    taskitem.h \
    tasklist.h

PRECOMPILED_HEADER = \
    erode_stable.h

LIBS += -lsystemd

include(../../qmake/liboxide.pri)
