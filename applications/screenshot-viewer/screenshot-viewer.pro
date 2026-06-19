QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += precompile_header
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

SOURCES += \
        main.cpp

TARGET = anxiety
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.anxiety.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/image.png
icons.path = $$ICONS_INSTALL_PATH
INSTALLS += icons

HEADERS += \
    controller.h \
    screenshotlist.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    anxiety_stable.h

include(../../qmake/liboxide.pri)
