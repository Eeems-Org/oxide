QT += gui
QT += quick
QT += dbus
QT += network

CONFIG += c++17
CONFIG += qml_debug
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

SOURCES += \
        main.cpp \
        terminalview.cpp

HEADERS += \
        terminalview.h

TARGET = wither
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = codes.eeems.wither.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.path = $$ICONS_INSTALL_PATH
icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/wither.png
INSTALLS += icons


RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)
include(../../qmake/libghostty-vt.pri)
