QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

INCLUDEPATH += ../../shared

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/powerapi.xml
DBUS_INTERFACES += ../../interfaces/wifiapi.xml
DBUS_INTERFACES += ../../interfaces/network.xml
DBUS_INTERFACES += ../../interfaces/bss.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml
DBUS_INTERFACES += ../../interfaces/systemapi.xml
DBUS_INTERFACES += ../../interfaces/screenapi.xml
DBUS_INTERFACES += ../../interfaces/screenshot.xml
DBUS_INTERFACES += ../../interfaces/notificationapi.xml
DBUS_INTERFACES += ../../interfaces/notification.xml

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

LIBS += -L$$PWD/../../.build/sentry/lib -lsentry -ldl -lcurl -lbreakpad_client
INCLUDEPATH += $$PWD/../../.build/sentry/include
DEPENDPATH += $$PWD/../../.build/sentry/lib

library.files = ../../.build/sentry/libsentry.so
library.path = /opt/lib
INSTALLS += library

LIBS += -L$$PWD/../../.build/liboxide -lliboxide
INCLUDEPATH += $$PWD/../../shared/liboxide
DEPENDPATH += $$PWD/../../shared/liboxide

LIBS += -L$$PWD/../../.build/liboxide -lliboxide
INCLUDEPATH += $$PWD/../../shared/liboxide
DEPENDPATH += $$PWD/../../shared/liboxide

QMAKE_RPATHDIR += /lib /usr/lib /opt/lib /opt/usr/lib

HEADERS += \
    json.h \
    slothandler.h
