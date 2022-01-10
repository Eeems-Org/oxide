QT += quick
CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    ../../shared/eventfilter.cpp

RESOURCES += qml.qrc

# Default rules for deployment.
target.path = /opt/bin
!isEmpty(target.path): INSTALLS += target

icons.files += \
    ../../assets/etc/draft/icons/erode.svg \
    ../../assets/etc/draft/icons/erode-splash.png
icons.path = /opt/etc/draft/icons
INSTALLS += icons

HEADERS += \
    controller.h \
    taskitem.h \
    ../../shared/eventfilter.h \
    tasklist.h

INCLUDEPATH += $$PWD/../../shared
INCLUDEPATH += ../../shared
DEPENDPATH += $$PWD/../../shared

LIBS += -lsystemd

LIBS += -L$$PWD/../../shared/ -lqsgepaper
INCLUDEPATH += $$PWD/../../shared
DEPENDPATH += $$PWD/../../shared

exists($$PWD/../../.build/sentry) {
    LIBS += -L$$PWD/../../.build/sentry/lib -lsentry -ldl -lcurl -lbreakpad_client
    INCLUDEPATH += $$PWD/../../.build/sentry/include
    DEPENDPATH += $$PWD/../../.build/sentry/lib

    library.files = ../../.build/sentry/libsentry.so
    library.path = /opt/lib
    INSTALLS += library
}

LIBS += -L$$PWD/../../.build/liboxide -lliboxide
INCLUDEPATH += $$PWD/../../shared/liboxide
DEPENDPATH += $$PWD/../../shared/liboxide

QMAKE_RPATHDIR += /lib /usr/lib /opt/lib /opt/usr/lib

VERSION = 2.3
