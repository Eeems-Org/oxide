QT += dbus
QT += quick
QT += gui

CONFIG += c++17
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CFLAGS += -std=c99

SOURCES += \
    connection.cpp \
    dbusinterface.cpp \
    main.cpp


configFile.files = ../../assets/etc/dbus-1/system.d/codes.eeems.blight.conf
configFile.path = /etc/dbus-1/system.d/
INSTALLS += configFile

TARGET = blight
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

HEADERS += \
    connection.h \
    dbusinterface.h

INCLUDEPATH += ../../shared/mxcfb

include(../../qmake/liboxide.pri)

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

RESOURCES += \
    qml.qrc

DISTFILES += \
    ../../assets/etc/dbus-1/system.d/codes.eeems.blight.conf \
    generate_xml.sh
