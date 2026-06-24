QT -= gui
QT += dbus

CONFIG += c++11 console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

HEADERS +=

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml

TARGET = update-desktop-database
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

include(../../qmake/liboxide.pri)
