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
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.ether.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.path = $$ICONS_INSTALL_PATH
icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/ether.png
INSTALLS += icons

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)

HEADERS +=
