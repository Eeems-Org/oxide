QT += gui
QT += quick
QT += dbus

CONFIG += c++11
CONFIG += qml_debug
CONFIG += qtquickcompiler
CONFIG += precompile_header

SOURCES += \
        appitem.cpp \
        main.cpp

TARGET = corrupt
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.corrupt.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

INCLUDEPATH += ../../shared
HEADERS += \
    appitem.h \
    controller.h \
    screenprovider.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    corrupt_stable.h

include(../../qmake/liboxide.pri)
