QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/systemapi.xml
DBUS_INTERFACES += ../../interfaces/screenapi.xml
DBUS_INTERFACES += ../../interfaces/screenshot.xml
DBUS_INTERFACES += ../../interfaces/notificationapi.xml
DBUS_INTERFACES += ../../interfaces/notification.xml

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)

TARGET = fret
include(../../qmake/common.pri)
