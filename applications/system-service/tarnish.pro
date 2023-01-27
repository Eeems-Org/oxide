QT += dbus

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
    event_device.cpp \
    network.cpp \
    notification.cpp \
    screenapi.cpp \
    screenshot.cpp \
    systemapi.cpp \
    wlan.cpp \
    wpa_supplicant.cpp \
    main.cpp

TARGET=tarnish

target.path = /opt/bin
INSTALLS += target

configFile.files = ../../assets/etc/dbus-1/system.d/*
configFile.path =  /etc/dbus-1/system.d/
INSTALLS += configFile

service.files = ../../assets/etc/systemd/system/tarnish.service
service.path = /etc/systemd/system/
INSTALLS += service

applications.files = ../../assets/opt/usr/share/applications/*
applications.path = /opt/usr/share/applications/
INSTALLS += applications

system(qdbusxml2cpp -N -p wpa_supplicant.h:wpa_supplicant.cpp fi.w1.wpa_supplicant1.xml)

DBUS_INTERFACES += org.freedesktop.login1.xml

HEADERS += \
    apibase.h \
    application.h \
    appsapi.h \
    bss.h \
    buttonhandler.h \
    dbusservice.h \
    digitizerhandler.h \
    event_device.h \
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
    ../../assets/opt/usr/share/applications/codes.eeems.anxiety.oxide \
    ../../assets/opt/usr/share/applications/codes.eeems.corrupt.oxide \
    fi.w1.wpa_supplicant1.xml \
    generate_xml.sh \
    org.freedesktop.login1.xml

LIBS += -L$$PWD/../../shared/epaper -lqsgepaper
INCLUDEPATH += $$PWD/../../shared/epaper

contains(DEFINES, SENTRY){
    exists($$PWD/../../.build/sentry/include/sentry.h) {
        LIBS += -L$$PWD/../../.build/sentry/lib -lsentry -ldl -lcurl -lbreakpad_client
        INCLUDEPATH += $$PWD/../../.build/sentry/include
        DEPENDPATH += $$PWD/../../.build/sentry/lib

        library.files = ../../.build/sentry/libsentry.so
        library.path = /opt/lib
        INSTALLS += library
    }else{
        error(You need to build sentry first)
    }
}

exists($$PWD/../../.build/liboxide/include/liboxide.h) {
    LIBS += -L$$PWD/../../.build/liboxide -lliboxide
    INCLUDEPATH += $$PWD/../../.build/liboxide/include
}else{
    error(You need to build liboxide first)
}

QMAKE_RPATHDIR += /lib /usr/lib /opt/lib /opt/usr/lib
QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

VERSION = 2.5
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
