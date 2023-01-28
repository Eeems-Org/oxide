QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += qtquickcompiler
CONFIG += precompile_header

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        appitem.cpp \
        main.cpp

TARGET = corrupt
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/screenapi.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml

INCLUDEPATH += ../../shared
HEADERS += \
    appitem.h \
    controller.h \
    screenprovider.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    corrupt_stable.h

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
