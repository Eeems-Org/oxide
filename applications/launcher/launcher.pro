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
    controller.cpp \
    appitem.cpp

RESOURCES += qml.qrc

TARGET = oxide
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

applications.files = codes.eeems.oxide.oxide
applications.path = $$APPLICATIONS_INSTALL_PATH
INSTALLS += applications

icons.files = ../../assets/opt/usr/share/icons/oxide/702x702/splash/oxide.png
icons.path  = $$SPLASH_INSTALL_PATH
INSTALLS += icons

configFile.files = ../../assets/etc/oxide.conf
configFile.path  = $$CONFIG_INSTALL_PATH
INSTALLS += configFile

DISTFILES += \
    ../../assets/etc/dbus-1/system.d/org.freedesktop.login1.conf \
    ../../assets/etc/oxide.conf

HEADERS += \
    controller.h \
    appitem.h \
    mxcfb.h \
    notificationlist.h \
    oxide_stable.h \
    wifinetworklist.h

PRECOMPILED_HEADER = \
    oxide_stable.h

include(../../qmake/liboxide.pri)
