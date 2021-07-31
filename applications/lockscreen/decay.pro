QT += gui
QT += quick
QT += dbus

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
        main.cpp \
        ../../shared/devicesettings.cpp \
        ../../shared/eventfilter.cpp

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

DBUS_INTERFACES += ../../interfaces/dbusservice.xml
DBUS_INTERFACES += ../../interfaces/systemapi.xml
DBUS_INTERFACES += ../../interfaces/powerapi.xml
DBUS_INTERFACES += ../../interfaces/wifiapi.xml
DBUS_INTERFACES += ../../interfaces/appsapi.xml
DBUS_INTERFACES += ../../interfaces/application.xml

INCLUDEPATH += ../../shared
HEADERS += \
    ../../shared/dbussettings.h \
    ../../shared/devicesettings.h \
    ../../shared/eventfilter.h \
    controller.h

RESOURCES += \
    qml.qrc

LIBS += -L$$PWD/../../docker-toolchain/qtcreator/files/libraries/ -lqsgepaper
INCLUDEPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries
DEPENDPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries
