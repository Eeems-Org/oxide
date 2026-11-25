QT += gui
QT += quick
QT += dbus

CONFIG += c++17
CONFIG += qml_debug
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

SOURCES += \
        main.cpp \
        controller.cpp

TARGET = defile
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = codes.eeems.defile.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/defile.png
icons.path = $$ICONS_INSTALL_PATH
INSTALLS += icons

HEADERS += \
    controller.h \
    stable.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    stable.h

include(../../qmake/liboxide.pri)
