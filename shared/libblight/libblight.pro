QT =
TEMPLATE = lib
DEFINES += LIBBLIGHT_LIBRARY

CONFIG += c++11
CONFIG += warn_on
CONFIG += precompile_header
CONFIG += create_pc
CONFIG += create_prl
CONFIG += no_install_prl

SOURCES += \
    clock.cpp \
    connection.cpp \
    dbus.cpp \
    debug.cpp \
    libblight.cpp \
    socket.cpp \
    types.cpp

HEADERS += \
    clock.h \
    connection.h \
    concurrentqueue.h \
    dbus.h \
    debug.h \
    libblight_global.h \
    libblight.h \
    meta.h \
    socket.h \
    types.h

PRECOMPILED_HEADER = \
    libblight_stable.h

LIBS += -lsystemd

include(../../qmake/common.pri)
RELATIVE_PWD = $$system(realpath --canonicalize-missing --relative-to $$OUT_PWD $$PWD)
include(../../qmake/libblight_protocol.pri)

libblight_libblight_h.target = include/libblight/libblight.h
for(h, HEADERS){
    libblight_libblight_h.depends += $${RELATIVE_PWD}/$${h}
}
libblight_libblight_h.commands = \
    mkdir -p include/libblight && \
    echo $$HEADERS | xargs -rn1 | xargs -rI {} cp $$PWD/{} include/libblight/ && \
    echo $$DBUS_INTERFACES | xargs -rn1 | xargs -rI {} basename \"{}\" .xml | xargs -rI {} cp $$OUT_PWD/\"{}\"_interface.h include/libblight/

libblight_h.target = include/libblight.h
libblight_h.depends = libblight_libblight_h
libblight_h.commands = \
    echo \\$$LITERAL_HASH"pragma once" > include/libblight.h && \
    echo \"$$LITERAL_HASH"include \\\"libblight/libblight.h\\\"\"" >> include/libblight.h

libblight_h_install.files = \
    include/libblight.h \
    include/libblight
libblight_h_install.depends = libblight_h
libblight_h_install.path = /opt/include/
INSTALLS += libblight_h_install

QMAKE_EXTRA_TARGETS += libblight_libblight_h libblight_h libblight_h_install
POST_TARGETDEPS += $$libblight_libblight_h.target $$libblight_h.target
QMAKE_CLEAN += $$libblight_h.target include/libblight/*.h

TARGET = blight
target.path = /opt/lib
INSTALLS += target

QMAKE_PKGCONFIG_NAME = libblight
QMAKE_PKGCONFIG_DESCRIPTION = Shared library for using Oxide\'s screen compositor
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_PREFIX = /opt
QMAKE_PKGCONFIG_LIBDIR = /opt/lib
QMAKE_PKGCONFIG_INCDIR = /opt/include
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

DISTFILES += \
    Doxyfile \
    Makefile
