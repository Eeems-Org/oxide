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

icons.files += \
    ../../assets/etc/draft/icons/image.svg \
    ../../assets/etc/draft/icons/anxiety-splash.png

icons.path = /opt/etc/draft/icons
INSTALLS += icons

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
