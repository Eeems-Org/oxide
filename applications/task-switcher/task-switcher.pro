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
        appitem.cpp \
        main.cpp

TARGET = corrupt
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = codes.eeems.corrupt.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

INCLUDEPATH += ../../shared
HEADERS += \
    appitem.h \
    controller.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    corrupt_stable.h

include(../../qmake/liboxide.pri)
