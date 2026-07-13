QT -= gui
QT += quick
QT += dbus
QT += core_private
QT += gui-private

TEMPLATE = lib
DEFINES += LIBOXIDE_LIBRARY

CONFIG += c++11
CONFIG += warn_on
CONFIG += precompile_header
CONFIG += create_pc
CONFIG += create_prl
CONFIG += no_install_prl
CONFIG += qmltypes

QML_IMPORT_NAME = codes.eeems.oxide
QML_IMPORT_MAJOR_VERSION = 2

SOURCES += \
    applications.cpp \
    debug.cpp \
    devicesettings.cpp \
    event_device.cpp \
    eventfilter.cpp \
    json.cpp \
    liboxide.cpp \
    oxide_sentry.cpp \
    oxideqml.cpp \
    power.cpp \
    settingsfile.cpp \
    sharedsettings.cpp \
    slothandler.cpp \
    socketpair.cpp \
    sysobject.cpp \
    signalhandler.cpp \
    threading.cpp \
    udev.cpp \
    xochitlsettings.cpp \
    dbus_types.cpp

HEADERS += \
    applications.h \
    dbus.h \
    debug.h \
    devicesettings.h \
    event_device.h \
    eventfilter.h \
    liboxide_global.h \
    liboxide.h \
    meta.h \
    oxide_sentry.h \
    oxideqml.h \
    power.h \
    json.h \
    settingsfile.h \
    sharedsettings.h \
    slothandler.h \
    socketpair.h \
    sysobject.h \
    signalhandler.h \
    threading.h \
    udev.h \
    xochitlsettings.h \
    dbus_types.h

PRECOMPILED_HEADER = \
    liboxide_stable.h

DBUS_INTERFACES += \
    ../../interfaces/dbusservice.xml \
    ../../interfaces/powerapi.xml \
    ../../interfaces/wifiapi.xml \
    ../../interfaces/network.xml \
    ../../interfaces/bss.xml \
    ../../interfaces/appsapi.xml \
    ../../interfaces/application.xml \
    ../../interfaces/systemapi.xml \
    ../../interfaces/screenapi.xml \
    ../../interfaces/screenshot.xml \
    ../../interfaces/notificationapi.xml \
    ../../interfaces/notification.xml \
    ../../interfaces/frontlightapi.xml

blight.files = ../../interfaces/blight.xml
blight.header_flags = -i dbus_types.h
DBUS_INTERFACES += blight

include(../../qmake/common.pri)
PKGCONFIG += libsystemd
PKGCONFIG += libudev
RELATIVE_PWD = $$system(realpath --canonicalize-missing --relative-to  $$OUT_PWD $$PWD)

liboxide_liboxide_h.target = include/liboxide/liboxide.h
for(h, HEADERS){
    liboxide_liboxide_h.depends += $${RELATIVE_PWD}/$${h}
}
INTERFACE_HEADERS = $$system(echo $$DBUS_INTERFACES | xargs -rn1 | xargs -rI {} basename {} .xml | xargs -rI {} echo {}_interface.h)
for(h, INTERFACE_HEADERS){
    liboxide_liboxide_h.depends += $$h
}
liboxide_liboxide_h.commands = \
    mkdir -p include/liboxide && \
    sed -i \'s/define OXIDE_VERSION .*/define OXIDE_VERSION \"$$VERSION\"/\' $$_PRO_FILE_PWD_/meta.h && \
    echo $$HEADERS | xargs -rn1 | xargs -rI {} cp $$PWD/{} include/liboxide/ && \
    mv include/liboxide/oxide_sentry.h include/liboxide/sentry.h && \
    echo $$DBUS_INTERFACES | xargs -rn1 | xargs -rI {} basename \"{}\" .xml | xargs -rI {} cp $$OUT_PWD/\"{}\"_interface.h include/liboxide/

liboxide_h.target = include/liboxide.h
liboxide_h.depends = liboxide_liboxide_h
liboxide_h.commands = \
    echo \\$$LITERAL_HASH"pragma once" > include/liboxide.h && \
    echo \"$$LITERAL_HASH"include \\\"liboxide/liboxide.h\\\"\"" >> include/liboxide.h

liboxide_h_install.files = \
    include/liboxide.h \
    include/liboxide
liboxide_h_install.depends = liboxide_h
liboxide_h_install.path = $$INCLUDE_INSTALL_PATH
INSTALLS += liboxide_h_install

QMAKE_EXTRA_TARGETS += liboxide_liboxide_h liboxide_h liboxide_h_install
POST_TARGETDEPS += $$liboxide_liboxide_h.target $$liboxide_h.target
QMAKE_CLEAN += $$liboxide_h.target include/liboxide/*.h

TARGET = oxide
target.path = $$LIB_INSTALL_PATH
INSTALLS += target

linux-oe-g++{
    INCLUDEPATH += ../../shared/mxcfb
    include(../../qmake/epaper.pri)
}
include(../../qmake/cpptrace-sentry.pri)
DEFINES += LIBBLIGHT_PRIVATE
include(../../qmake/libblight.pri)

QMAKE_PKGCONFIG_NAME = liboxide
QMAKE_PKGCONFIG_DESCRIPTION = Shared library for Oxide application development
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_PREFIX = $$ROOT_INSTALL_PATH
QMAKE_PKGCONFIG_LIBDIR = $$LIB_INSTALL_PATH
QMAKE_PKGCONFIG_INCDIR = $$INCLUDE_INSTALL_PATH
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

RESOURCES += \
    oxide.qrc

DISTFILES += \
    Doxyfile \
    Makefile \
    OxideMenu.qml \
    OxideStatusIcon.qml \
    OxideWindow.qml
