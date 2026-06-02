QT += testlib
QT += core

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

SOURCES += \
    main.cpp \
    test_edit.cpp \
    test_proc_stat.cpp \
    ../../applications/desktop-file-edit/edit.cpp

HEADERS += \
    autotest.h \
    test_edit.h \
    test_proc_stat.h \
    ../../applications/desktop-file-edit/edit.h

include(../../qmake/common.pri)