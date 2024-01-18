QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += qtquickcompiler

SOURCES += \
        main.cpp

TARGET = test
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.decay.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)
include(../../qmake/libblight.pri)
