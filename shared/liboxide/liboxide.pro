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
    debug.cpp \
    eventfilter.cpp \
    json.cpp \
    liboxide.cpp \
    oxide_sentry.cpp \
    power.cpp \
    settingsfile.cpp \
    slothandler.cpp \
    sysobject.cpp \
    signalhandler.cpp

HEADERS += \
    debug.h \
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
    signalhandler.h

PRECOMPILED_HEADER = \
    liboxide_stable.h

LIBS += -lsystemd

include.target = include/liboxide
include.commands = \
    mkdir -p include/liboxide && \
    echo $$HEADERS | xargs -rn1 | xargs -rI {} cp $$PWD/{} include/liboxide/

liboxide_h.target = include/liboxide.h
liboxide_h.depends += include
liboxide_h.commands = \
    echo \\$$LITERAL_HASH"ifndef LIBOXIDE" > include/liboxide.h && \
    echo \\$$LITERAL_HASH"define LIBOXIDE" >> include/liboxide.h && \
    echo \"$$LITERAL_HASH"include \\\"liboxide/liboxide.h\\\"\"" >> include/liboxide.h && \
    echo \\$$LITERAL_HASH"endif // LIBOXIDE" >> include/liboxide.h


QMAKE_EXTRA_TARGETS += include liboxide_h
POST_TARGETDEPS += include/liboxide.h

target.path = /opt/usr/lib
!isEmpty(target.path): INSTALLS += target

include(../../qmake/epaper.pri)
include(../../qmake/sentry.pri)
include(../../qmake/common.pri)
