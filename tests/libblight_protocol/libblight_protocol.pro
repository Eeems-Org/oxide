QT += testlib
QT -= gui

target.path = /opt/share/tests
INSTALLS += target

CONFIG += qt
CONFIG += console
CONFIG += warn_on
CONFIG += depend_includepath
CONFIG += testcase
CONFIG += no_testcase_installs
CONFIG -= app_bundle

TEMPLATE = app

SOURCES +=  \
    main.cpp \
    test.c

HEADERS += \
    autotest.h \
    test.h

include(../../qmake/common.pri)
include(../../qmake/libblight.pri)
include(../../qmake/libblight_protocol.pri)
