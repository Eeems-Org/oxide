QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
    application.cpp \
    appsapi.cpp \
    bss.cpp \
    buttonhandler.cpp \
    event_device.cpp \
    fb2png.cpp \
    network.cpp \
    sysobject.cpp \
    systemapi.cpp \
    wlan.cpp \
    wpa_supplicant.cpp \
    main.cpp \
    ../../shared/settings.cpp

TARGET=tarnish

target.path = /opt/bin
INSTALLS += target

configFile.files = ../../assets/etc/dbus-1/system.d/*
configFile.path =  /etc/dbus-1/system.d/
INSTALLS += configFile

service.files = ../../assets/etc/systemd/system/tarnish.service
service.path = /etc/systemd/system/
INSTALLS += service

system(qdbusxml2cpp -N -p wpa_supplicant.h:wpa_supplicant.cpp fi.w1.wpa_supplicant1.xml)

DBUS_INTERFACES += org.freedesktop.login1.xml

INCLUDEPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries
INCLUDEPATH += ../../shared
DEPENDPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries

HEADERS += \
    apibase.h \
    application.h \
    appsapi.h \
    bss.h \
    buttonhandler.h \
    dbusservice.h \
    dbussettings.h \
    digitizerhandler.h \
    event_device.h \
    fb2png.h \
    inputmanager.h \
    mxcfb.h \
    network.h \
    powerapi.h \
    screenapi.h \
    signalhandler.h \
    stb_image.h \
    stb_image_write.h \
    supplicant.h \
    sysobject.h \
    systemapi.h \
    wifiapi.h \
    wlan.h \
    wpa_supplicant.h \
    settings.h

linux-oe-g++ {
    LIBS += -lqsgepaper
    LIBS += -lpng16
    LIBS += -lsystemd
    LIBS += -lz
}

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

DISTFILES += \
    fi.w1.wpa_supplicant1.xml \
    generate_xml.sh \
    org.freedesktop.login1.xml

RESOURCES +=
