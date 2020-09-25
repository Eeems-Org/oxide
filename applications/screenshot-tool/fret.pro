QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/systemapi.xml
DBUS_INTERFACES += ../../interfaces/screenapi.xml


INCLUDEPATH += ../../shared
HEADERS += \
    ../../shared/dbussettings.h
