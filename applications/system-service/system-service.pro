QT += quick
QT += testlib

CONFIG += c++17
CONFIG += console
CONFIG -= app_bundle
CONFIG += precompile_header

QMAKE_CFLAGS += -std=c99

SOURCES += \
    apibase.cpp \
    application.cpp \
    appsapi.cpp \
    bss.cpp \
    childentry.cpp \
    dbusservice.cpp \
    eventlistener.cpp \
    guiapi.cpp \
    guithread.cpp \
    network.cpp \
    notification.cpp \
    notificationapi.cpp \
    powerapi.cpp \
    screenapi.cpp \
    screenshot.cpp \
    socketpair.cpp \
    systemapi.cpp \
    wifiapi.cpp \
    window.cpp \
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
    childentry.h \
    dbusservice.h \
    digitizerhandler.h \
    eventlistener.h \
    fifohandler.h \
    guiapi.h \
    guithread.h \
    network.h \
    notification.h \
    notificationapi.h \
    powerapi.h \
    screenapi.h \
    screenshot.h \
    socketpair.h \
    supplicant.h \
    systemapi.h \
    tarnish_stable.h \
    wifiapi.h \
    window.h \
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

INCLUDEPATH += ../../shared/mxcfb

include(../../qmake/liboxide.pri)

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh
