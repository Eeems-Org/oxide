QT += qml
QT += quick

CONFIG += c++11
CONFIG += qml_debug
CONFIG += precompile_header
CONFIG(release, debug|release){
    CONFIG += qtquickcompiler
}

TARGET = codes.eeems.text
include(../../qmake/common.pri)
target.path = $$ROOT_INSTALL_PATH/share/oxide/libexec
INSTALLS += target

applications.files = codes.eeems.text.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

SOURCES += \
    main.cpp \
    lazylinemodel.cpp

HEADERS += \
    lazylinemodel.h

RESOURCES += \
    qml.qrc

include(../../qmake/liboxide.pri)
