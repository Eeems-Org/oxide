QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += main.cpp \
    bss.cpp \
    buttonhandler.cpp \
    fb2png.cpp \
    network.cpp \
    sysobject.cpp \
    wlan.cpp \
    wpa_supplicant.cpp

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


HEADERS += \
    bss.h \
    buttonhandler.h \
    dbusservice.h \
    dbussettings.h \
    fb2png.h \
    inputmanager.h \
    mxcfb.h \
    network.h \
    powerapi.h \
    supplicant.h \
    sysobject.h \
    wifiapi.h \
    wlan.h \
    wpa_supplicant.h

linux-oe-g++ {
    LIBS += -lpng16
}

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

DISTFILES += \
    fi.w1.wpa_supplicant1.xml \
    generate_xml.sh

RESOURCES +=