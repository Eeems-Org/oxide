QT += dbus
QT += quick
QT += gui

CONFIG += c++17
CONFIG -= app_bundle
CONFIG += qml_debug
CONFIG += qtquickcompiler
CONFIG += qmltypes # disables all the APIs deprecated before Qt 6.0.0

QML_IMPORT_NAME = codes.eeems.blight
QML_IMPORT_PATH += .
QML_IMPORT_MAJOR_VERSION = 1

QMAKE_CFLAGS += -std=c99

SOURCES += \
    connection.cpp \
    dbusinterface.cpp \
    evdevdevice.cpp \
    evdevhandler.cpp \
    guithread.cpp \
    main.cpp \
    surface.cpp \
    surfacewidget.cpp


configFile.files = ../../assets/etc/dbus-1/system.d/codes.eeems.blight.conf
configFile.path = /etc/dbus-1/system.d/
INSTALLS += configFile

TARGET = blight
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

HEADERS += \
    connection.h \
    dbusinterface.h \
    evdevdevice.h \
    evdevhandler.h \
    guithread.h \
    surface.h \
    surfacewidget.h

INCLUDEPATH += ../../shared/mxcfb

include(../../qmake/liboxide.pri)
include(../../qmake/libblight.pri)

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh

RESOURCES += \
    qml.qrc

DISTFILES += \
    ../../assets/etc/dbus-1/system.d/codes.eeems.blight.conf \
    generate_xml.sh
