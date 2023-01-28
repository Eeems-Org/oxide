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

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/systemapi.xml
DBUS_INTERFACES += ../../interfaces/powerapi.xml
DBUS_INTERFACES += ../../interfaces/wifiapi.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml

HEADERS += \
    controller.h

RESOURCES += \
    qml.qrc

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)

TARGET = decay
include(../../qmake/common.pri)
