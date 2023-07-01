QT -= gui
QT += quick
QT += dbus

TEMPLATE = lib
DEFINES += LIBOXIDE_LIBRARY

CONFIG += c++11
CONFIG += warn_on
CONFIG += precompile_header

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    applications.cpp \
    debug.cpp \
    event_device.cpp \
    eventfilter.cpp \
    json.cpp \
    liboxide.cpp \
    oxide_sentry.cpp \
    power.cpp \
    settingsfile.cpp \
    slothandler.cpp \
    sysobject.cpp \
    signalhandler.cpp \
    udev.cpp

HEADERS += \
    applications.h \
    dbus.h \
    debug.h \
    event_device.h \
    eventfilter.h \
    liboxide_global.h \
    liboxide.h \
    meta.h \
    oxide_sentry.h \
    power.h \
    json.h \
    settingsfile.h \
    slothandler.h \
    sysobject.h \
    signalhandler.h \
    udev.h

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

LIBS += -lsystemd -ludev

liboxide_liboxide_h.target = include/liboxide/liboxide.h
liboxide_liboxide_h.commands = \
    mkdir -p include/liboxide && \
    echo $$HEADERS | xargs -rn1 | xargs -rI {} cp $$PWD/{} include/liboxide/ && \
    echo $$DBUS_INTERFACES | xargs -rn1 | xargs -rI {} basename \"{}\" .xml | xargs -rI {} cp $$OUT_PWD/\"{}\"_interface.h include/liboxide/

liboxide_h.target = include/liboxide.h
liboxide_h.depends += liboxide_liboxide_h
liboxide_h.commands = \
    echo \\$$LITERAL_HASH"pragma once" > include/liboxide.h && \
    echo \"$$LITERAL_HASH"include \\\"liboxide/liboxide.h\\\"\"" >> include/liboxide.h

clean_headers.target = include/.clean-target
clean_headers.commands = rm -rf include

QMAKE_EXTRA_TARGETS += liboxide_liboxide_h liboxide_h clean_headers
PRE_TARGETDEPS += $$clean_headers.target
POST_TARGETDEPS += $$liboxide_liboxide_h.target $$liboxide_h.target
QMAKE_CLEAN += $$liboxide_h.target include/liboxide/*.h

include(../../qmake/common.pri)
target.path = /opt/lib
INSTALLS += target

include(../../qmake/epaper.pri)
include(../../qmake/sentry.pri)
