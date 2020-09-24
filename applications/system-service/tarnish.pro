QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
    application.cpp \
    bss.cpp \
    buttonhandler.cpp \
    fb2png.cpp \
    network.cpp \
    sysobject.cpp \
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

suspend.files = ../../assets/lib/systemd/system-sleep/tarnish
suspend.path = /lib/systemd/system-sleep/
INSTALLS += suspend

system(qdbusxml2cpp -N -p wpa_supplicant.h:wpa_supplicant.cpp fi.w1.wpa_supplicant1.xml)

INCLUDEPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries
DEPENDPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries

HEADERS += \
    apibase.h \
    application.h \
    appsapi.h \
    bss.h \
    buttonhandler.h \
    dbusservice.h \
    dbussettings.h \
    fb2png.h \
    inputmanager.h \
    mxcfb.h \
    network.h \
    powerapi.h \
    signalhandler.h \
    stb_image.h \
    stb_image_write.h \
    supplicant.h \
    sysobject.h \
    wifiapi.h \
    wlan.h \
    wpa_supplicant.h

linux-oe-g++ {
    LIBS += -lqsgepaper
    LIBS += -lpng16
    LIBS += -lsystemd
    LIBS += -lz
}

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

DISTFILES += \
    ../../assets/lib/systemd/system-sleep/tarnish \
    fi.w1.wpa_supplicant1.xml \
    generate_xml.sh

RESOURCES += \
    qml.qrc
