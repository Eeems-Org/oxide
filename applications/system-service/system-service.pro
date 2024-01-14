QT += dbus
QT += quick

CONFIG += c++17
CONFIG += console
CONFIG -= app_bundle
CONFIG += precompile_header

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CFLAGS += -std=c99

SOURCES += \
    apibase.cpp \
    application.cpp \
    appsapi.cpp \
    bss.cpp \
    buttonhandler.cpp \
    eventlistener.cpp \
    network.cpp \
    notification.cpp \
    screenapi.cpp \
    screenshot.cpp \
    systemapi.cpp \
    wlan.cpp \
    wpa_supplicant.cpp \
    main.cpp

TARGET = tarnish
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

configFile.files = ../../assets/etc/dbus-1/system.d/codes.eeems.oxide.conf
configFile.path =  /etc/dbus-1/system.d/
INSTALLS += configFile

service.files = ../../assets/etc/systemd/system/tarnish.service
service.path = /etc/systemd/system/
INSTALLS += service

applications.files = ../../assets/opt/usr/share/applications/xochitl.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/xochitl.png
icons.path = /opt/usr/share/icons/oxide/48x48/apps
INSTALLS += icons

system(qdbusxml2cpp -N -p wpa_supplicant.h:wpa_supplicant.cpp fi.w1.wpa_supplicant1.xml)

DBUS_INTERFACES += org.freedesktop.login1.xml

HEADERS += \
    apibase.h \
    application.h \
    appsapi.h \
    bss.h \
    buttonhandler.h \
    controller.h \
    dbusservice.h \
    digitizerhandler.h \
    eventlistener.h \
    fifohandler.h \
    mxcfb.h \
    network.h \
    notification.h \
    notificationapi.h \
    powerapi.h \
    screenapi.h \
    screenshot.h \
    supplicant.h \
    systemapi.h \
    tarnish_stable.h \
    wifiapi.h \
    wlan.h \
    wpa_supplicant.h

PRECOMPILED_HEADER = \
    tarnish_stable.h

LIBS += -lpng16
LIBS += -lsystemd
LIBS += -lz

DISTFILES += \
    fi.w1.wpa_supplicant1.xml \
    generate_xml.sh \
    org.freedesktop.login1.xml

include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

RESOURCES += \
    qml.qrc
