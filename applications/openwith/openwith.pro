QT += qml
QT += quick
QT += dbus
QT += gui

CONFIG += c++11
CONFIG += qml_debug
CONFIG += precompile_header
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

TARGET = codes.eeems.openwith
include(../../qmake/common.pri)
target.path = $$ROOT_INSTALL_PATH/share/oxide/libexec
INSTALLS += target

applications.files = codes.eeems.openwith.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

SOURCES += \
    main.cpp \
    controller.cpp

HEADERS += \
    controller.h

PRECOMPILED_HEADER = \
    openwith_stable.h

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)