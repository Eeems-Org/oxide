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
    sysobject.cpp \
    signalhandler.cpp \
    xochitlsettings.cpp

HEADERS += \
    applications.h \
    dbus.h \
    debug.h \
    devicesettings.h \
    epaper.h \
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
    sysobject.h \
    signalhandler.h \
    xochitlsettings.h

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
    ../../interfaces/notification.xml

LIBS += -lsystemd

include(../../qmake/common.pri)

liboxide_liboxide_h.target = include/liboxide/liboxide.h
liboxide_liboxide_h.commands = \
    mkdir -p include/liboxide && \
    sed -i \'s/define OXIDE_VERSION .*/define OXIDE_VERSION \"$$VERSION\"/\' $$_PRO_FILE_PWD_/meta.h && \
    echo $$HEADERS | xargs -rn1 | xargs -rI {} cp $$PWD/{} include/liboxide/ && \
    mv include/liboxide/oxide_sentry.h include/liboxide/sentry.h && \
    echo $$DBUS_INTERFACES | xargs -rn1 | xargs -rI {} basename \"{}\" .xml | xargs -rI {} cp $$OUT_PWD/\"{}\"_interface.h include/liboxide/

liboxide_h.target = include/liboxide.h
liboxide_h.depends += liboxide_liboxide_h
liboxide_h.commands = \
    echo \\$$LITERAL_HASH"pragma once" > include/liboxide.h && \
    echo \"$$LITERAL_HASH"include \\\"liboxide/liboxide.h\\\"\"" >> include/liboxide.h

clean_headers.target = include/.clean-target
clean_headers.commands = rm -rf include

liboxide_h_install.files = \
    include/liboxide.h \
    include/liboxide
liboxide_h_install.depends = liboxide_h
liboxide_h_install.path = /opt/include/
INSTALLS += liboxide_h_install

linux-oe-g++{
    epframebuffer_h_install.files = ../epaper/epframebuffer.h
    epframebuffer_h_install.path = /opt/include
    INSTALLS += epframebuffer_h_install
}

QMAKE_EXTRA_TARGETS += liboxide_liboxide_h liboxide_h clean_headers liboxide_h_install
PRE_TARGETDEPS += $$clean_headers.target
POST_TARGETDEPS += $$liboxide_liboxide_h.target $$liboxide_h.target
QMAKE_CLEAN += $$liboxide_h.target include/liboxide/*.h

TARGET = oxide
target.path = /opt/lib
INSTALLS += target

INCLUDEPATH += ../../shared/mxcfb

include(../../qmake/epaper.pri)
include(../../qmake/sentry.pri)

QMAKE_PKGCONFIG_NAME = liboxide
QMAKE_PKGCONFIG_DESCRIPTION = Shared library for Oxide application development
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_PREFIX = /opt
QMAKE_PKGCONFIG_LIBDIR = /opt/lib
QMAKE_PKGCONFIG_INCDIR = /opt/include
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

RESOURCES += \
    oxide.qrc

DISTFILES += \
    OxideMenu.qml \
    OxideWindow.qml
