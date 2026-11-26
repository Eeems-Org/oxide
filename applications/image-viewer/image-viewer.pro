QT += qml
QT += quick

CONFIG += c++11
CONFIG += qml_debug
CONFIG += precompile_header
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

TARGET = codes.eeems.image
include(../../qmake/common.pri)
target.path = $$ROOT_INSTALL_PATH/share/oxide/libexec
INSTALLS += target

applications.files = codes.eeems.image.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

SOURCES += \
    main.cpp

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)