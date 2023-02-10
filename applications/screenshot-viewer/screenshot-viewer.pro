QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += qtquickcompiler
CONFIG += precompile_header

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

TARGET = anxiety
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.anxiety.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/image.png
icons.path = /opt/usr/share/icons/oxide/48x48/apps
INSTALLS += icons

splash.files += ../../assets/opt/usr/share/icons/oxide/702x702/splash/anxiety.png
splash.path = /opt/usr/share/icons/oxide/702x702/splash
INSTALLS += splash

HEADERS += \
    controller.h \
    screenshotlist.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    anxiety_stable.h

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
