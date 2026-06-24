QT += quick
QT += dbus

CONFIG += c++11
CONFIG += precompile_header# disables all the APIs deprecated before Qt 6.0.0
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

SOURCES += main.cpp

RESOURCES += qml.qrc

TARGET = erode
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.erode.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.path = $$ICONS_INSTALL_PATH
icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/erode.png
INSTALLS += icons

HEADERS += \
    controller.h \
    taskitem.h \
    tasklist.h

PRECOMPILED_HEADER = \
    erode_stable.h

include(../../qmake/liboxide.pri)
PKGCONFIG += libsystemd
