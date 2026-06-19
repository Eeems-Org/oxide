QT += dbus
QT += quick
QT += gui
QT += input_support-private

CONFIG += c++17
CONFIG -= app_bundle
CONFIG += qml_debug
CONFIG += qmltypes
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

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


service.files = ../../assets/etc/systemd/system/blight.service
service.path = /etc/systemd/system/
INSTALLS += service

include(../../qmake/common.pri)

client.files = blight-client
client.path = $$BIN_INSTALL_PATH
INSTALLS += client

TARGET = blight
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

!system("pkg-config --exists libevdev"): error("Could not find libevdev package via pkg-config")
LIBS += -Wl,-Bstatic $$system("pkg-config --static --libs libevdev") -Wl,-Bdynamic
QMAKE_CXXFLAGS += $$system("pkg-config --cflags libevdev")

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
include(../../qmake/epaper.pri)

QMAKE_POST_LINK += sh $$_PRO_FILE_PWD_/generate_xml.sh


!contains(DEFINES, EPAPER){
    RESOURCES += \
        qml.qrc
}

DISTFILES += \
    ../../assets/etc/dbus-1/system.d/codes.eeems.blight.conf \
    ../../assets/etc/systemd/system/blight.service \
    generate_xml.sh \
    blight-client
