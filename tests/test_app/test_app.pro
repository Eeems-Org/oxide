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

TARGET = test_app
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = test_app.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)
include(../../qmake/libblight.pri)

HEADERS +=
