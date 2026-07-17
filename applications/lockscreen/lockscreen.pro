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

TARGET = decay
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = codes.eeems.decay.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

HEADERS += \
    controller.h

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)
