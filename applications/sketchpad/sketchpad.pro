QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

SOURCES += \
        main.cpp

TARGET = ether
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.ether.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

icons.path = /opt/usr/share/icons/oxide/48x48/apps/
icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/ether.png
INSTALLS += icons

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)

HEADERS +=
