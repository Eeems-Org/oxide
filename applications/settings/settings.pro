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

TARGET = codes.eeems.settings
include(../../qmake/common.pri)
target.path = $$ROOT_INSTALL_PATH/share/oxide/libexec
INSTALLS += target

applications.files = codes.eeems.settings.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

HEADERS += \
    controller.h \
    generalcontroller.h \
    displaycontroller.h \
    powercontroller.h \
    wificontroller.h \
    notificationscontroller.h \
    notificationlist.h \
    wifinetworklist.h

RESOURCES += \
    qml.qrc

PRECOMPILED_HEADER = \
    settings_stable.h

include(../../qmake/liboxide.pri)
