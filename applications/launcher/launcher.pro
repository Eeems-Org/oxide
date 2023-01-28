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
    main.cpp \
    controller.cpp \
    appitem.cpp


RESOURCES += qml.qrc

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/powerapi.xml
DBUS_INTERFACES += ../../interfaces/wifiapi.xml
DBUS_INTERFACES += ../../interfaces/network.xml
DBUS_INTERFACES += ../../interfaces/bss.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml
DBUS_INTERFACES += ../../interfaces/systemapi.xml
DBUS_INTERFACES += ../../interfaces/notificationapi.xml
DBUS_INTERFACES += ../../interfaces/notification.xml

icons.files = ../../assets/etc/draft/icons/oxide-splash.png
icons.path  = /opt/etc/draft/icons
INSTALLS += icons

configFile.files = ../../assets/etc/oxide.conf
configFile.path  = /opt/etc/
INSTALLS += configFile

DISTFILES += \
    ../../assets/etc/dbus-1/system.d/org.freedesktop.login1.conf \
    ../../assets/etc/oxide.conf

HEADERS += \
    controller.h \
    appitem.h \
    mxcfb.h \
    notificationlist.h \
    oxide_stable.h \
    wifinetworklist.h

PRECOMPILED_HEADER = \
    oxide_stable.h

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)

TARGET = oxide
include(../../qmake/common.pri)
