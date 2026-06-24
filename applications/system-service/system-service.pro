QT += dbus
QT += quick

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
    dbusservice.cpp \
    eventlistener.cpp \
    network.cpp \
    notification.cpp \
    notificationapi.cpp \
    powerapi.cpp \
    screenapi.cpp \
    screenshot.cpp \
    systemapi.cpp \
    wifiapi.cpp \
    wlan.cpp \
    wpa_supplicant.cpp \
    main.cpp

TARGET = tarnish
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

configFile.files = ../../assets/etc/dbus-1/system.d/codes.eeems.oxide.conf
configFile.path =  /etc/dbus-1/system.d/
INSTALLS += configFile

service.files = ../../assets/etc/systemd/system/tarnish.service
service.path = /etc/systemd/system/
INSTALLS += service

keyd.files = ../../assets/opt/etc/keyd/oxide.conf
keyd.path = $$CONFIG_INSTALL_PATH/keyd/
INSTALLS += keyd

launcherctl.files = ../../assets/opt/share/launcherctl/oxide
launcherctl.path = $$SHARE_INSTALL_PATH/launcherctl/
INSTALLS += launcherctl

runner.files = runner
runner.path = $$ROOT_INSTALL_PATH/share/oxide/libexec/
INSTALLS += runner

applications.files = \
    ../../assets/opt/usr/share/applications/xochitl.oxide \
    ../../assets/opt/usr/share/applications/xovi.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/xochitl.png
icons.path = $$ICONS_INSTALL_PATH
INSTALLS += icons

system(qdbusxml2cpp -N -p wpa_supplicant.h:wpa_supplicant.cpp fi.w1.wpa_supplicant1.xml)
system(bash -c 'cd ../../; clang-format --fallback-style=mozilla -i applications/system-service/wpa_supplicant.h:wpa_supplicant.cpp applications/system-service/wpa_supplicant.h:wpa_supplicant.h')

DBUS_INTERFACES += org.freedesktop.login1.xml

HEADERS += \
    apibase.h \
    application.h \
    appsapi.h \
    bss.h \
    controller.h \
    dbusservice.h \
    eventlistener.h \
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

PKGCONFIG += libsystemd
PKGCONFIG += libpng16
PKGCONFIG += zlib

DISTFILES += \
    fi.w1.wpa_supplicant1.xml \
    generate_xml.sh \
    org.freedesktop.login1.xml \
    runner \
    ../../assets/etc/dbus-1/system.d/codes.eeems.oxide.conf \
    ../../assets/etc/systemd/system/tarnish.service \
    ../../assets/opt/usr/share/applications/xochitl.oxide \
    ../../assets/opt/etc/keyd/oxide.conf

INCLUDEPATH += ../../shared/mxcfb

include(../../qmake/liboxide.pri)
include(../../qmake/libblight.pri)

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

RESOURCES += \
    qml.qrc
