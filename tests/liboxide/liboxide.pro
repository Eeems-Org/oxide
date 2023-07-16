QT += testlib
QT += gui

target.path = /opt/share/tests/liboxide/liboxide

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    test_QByteArray.cpp

include(../../qmake/common.pri)
include(../../qmake/liboxide.pri)
