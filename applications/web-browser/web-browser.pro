QT += gui
QT += quick
QT += webkitwidgets

CONFIG += c++11
CONFIG += qml_debug

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

linux-oe-g++ {
    LIBS += -lqsgepaper
}

SOURCES += \
        main.cpp

TARGET = mold
include(../../qmake/common.pri)
target.path = /opt/bin
INSTALLS += target

applications.files = ../../assets/opt/usr/share/applications/codes.eeems.mold.oxide
applications.path = /opt/usr/share/applications/
INSTALLS += applications

icons.files += ../../assets/opt/usr/share/icons/oxide/48x48/apps/web.png
icons.path = /opt/usr/share/icons/oxide/48x48/apps
INSTALLS += icons

splash.files += ../../assets/opt/usr/share/icons/oxide/702x702/splash/web.png
splash.path = /opt/usr/share/icons/oxide/702x702/splash
INSTALLS += splash

INCLUDEPATH += ../../shared
HEADERS += \
    ../../shared/dbussettings.h \
    ../../shared/devicesettings.h \
    ../../shared/eventfilter.h \
    controller.h \
    keyboardhandler.h

RESOURCES += \
    qml.qrc

include(../../qmake/epaper.pri)
include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
