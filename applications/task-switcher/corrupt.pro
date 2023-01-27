QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += qtquickcompiler

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        appitem.cpp \
        main.cpp

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/screenapi.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml

INCLUDEPATH += ../../shared
HEADERS += \
    appitem.h \
    controller.h \
    screenprovider.h

RESOURCES += \
    qml.qrc

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

VERSION = 2.5
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
