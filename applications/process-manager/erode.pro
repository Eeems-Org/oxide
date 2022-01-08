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
    ../../shared/devicesettings.cpp\
    ../../shared/eventfilter.cpp \
    ../../shared/sentry_settings.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

icons.files += \
    ../../assets/etc/draft/icons/erode.svg \
    ../../assets/etc/draft/icons/erode-splash.png
icons.path = /opt/etc/draft/icons
INSTALLS += icons

HEADERS += \
    ../../shared/sentry_settings.h \
    controller.h \
    taskitem.h \
    ../../shared/dbussettings.h \
    ../../shared/devicesettings.h\
    ../../shared/eventfilter.h \
    tasklist.h

INCLUDEPATH += $$PWD/../../shared
INCLUDEPATH += ../../shared
DEPENDPATH += $$PWD/../../shared


linux-oe-g++ {
    LIBS += -lqsgepaper
}

DISTFILES +=

LIBS += -L$$PWD/../../shared/ -lqsgepaper
INCLUDEPATH += $$PWD/../../shared
DEPENDPATH += $$PWD/../../shared

LIBS += -L$$PWD/../../.build/sentry/lib -lsentry -ldl -lcurl -lbreakpad_client
INCLUDEPATH += $$PWD/../../.build/sentry/include
DEPENDPATH += $$PWD/../../.build/sentry/lib

library.files = ../../.build/sentry/libsentry.so
library.path = /opt/lib
INSTALLS += library

QMAKE_RPATHDIR += /lib /usr/lib /opt/lib
