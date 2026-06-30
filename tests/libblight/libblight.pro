QT += testlib
QT -= gui

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
    test_connection.cpp \
    test_socket.cpp \
    test_types.cpp

HEADERS += \
    autotest.h \
    test_clock.h \
    test_connection.h \
    test_socket.h \
    test_types.h

include(../../qmake/common.pri)
include(../../qmake/libblight.pri)

target.path = $$TESTS_INSTALL_PATH
INSTALLS += target
