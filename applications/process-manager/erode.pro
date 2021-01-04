QT += quick
CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    controller.cpp \
    taskitem.cpp \
    ../../shared/devicesettings.cpp\
    ../../shared/eventfilter.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    controller.h \
    taskitem.h \
    ../../shared/dbussettings.h \
    ../../shared/devicesettings.h\
    ../../shared/eventfilter.h

INCLUDEPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries
INCLUDEPATH += ../../shared
DEPENDPATH += $$PWD/../../docker-toolchain/qtcreator/files/libraries


linux-oe-g++ {
    LIBS += -lqsgepaper
}

DISTFILES +=
