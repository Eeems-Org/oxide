QT += testlib
QT += gui

target.path = /opt/share/tests/liboxide/liboxide

CONFIG += qt
CONFIG += console
CONFIG += warn_on
CONFIG += depend_includepath
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    test_Math.cpp \
    test_QByteArray.cpp

include(../../qmake/common.pri)
include(../../qmake/liboxide.pri)

HEADERS += \
    autotest.h \
    test_Math.h \
    test_QByteArray.h
