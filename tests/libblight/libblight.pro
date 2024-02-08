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
    test_clock.cpp \
    test_socket.cpp \
    test_types.cpp

HEADERS += \
    autotest.h \
    test_clock.h \
    test_socket.h \
    test_types.h

include(../../qmake/common.pri)
include(../../qmake/libblight.pri)
