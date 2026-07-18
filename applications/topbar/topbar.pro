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
    main.cpp \
    controller.cpp

RESOURCES += qml.qrc

TARGET = stain
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = codes.eeems.stain.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

HEADERS += \
    controller.h \
    stain_stable.h

PRECOMPILED_HEADER = \
    stain_stable.h

include(../../qmake/liboxide.pri)
