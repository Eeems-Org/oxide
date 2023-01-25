QT -= gui
QT += quick
QT += dbus

TEMPLATE = lib
DEFINES += LIBOXIDE_LIBRARY

CONFIG += c++11
CONFIG += warn_on

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    debug.cpp \
    eventfilter.cpp \
    json.cpp \
    liboxide.cpp \
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
    power.h \
    json.h \
    settingsfile.h \
    slothandler.h \
    sysobject.h \
    signalhandler.h

LIBS += -lsystemd

INCLUDEPATH += ../../shared
LIBS += -L$$PWD/../../shared/ -lqsgepaper
INCLUDEPATH += $$PWD/../../shared
DEPENDPATH += $$PWD/../../shared

contains(DEFINES, SENTRY){
    exists($$PWD/../../.build/sentry) {
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

target.path = /opt/usr/lib
!isEmpty(target.path): INSTALLS += target

VERSION = 2.5
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
