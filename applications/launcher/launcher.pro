QT += gui
QT += quick

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

TARGET = oxide
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.oxide.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

icons.files = ../../assets/opt/usr/share/icons/oxide/702x702/splash/oxide.png
icons.path  = /opt/usr/share/icons/oxide/702x702/splash/
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

include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
