QT =
TEMPLATE = lib
DEFINES += LIBBLIGHT_PROTOCOL_LIBRARY

CONFIG += c++11
CONFIG += warn_on
CONFIG += create_pc
CONFIG += create_prl
CONFIG += no_install_prl

SOURCES += \
    _debug.cpp \
    libblight_protocol.cpp \
    socket.cpp

HEADERS += \
    _debug.h \
    libblight_protocol.h \
    libblight_protocol_global.h \
    socket.h

LIBS += -lsystemd

include(../../qmake/common.pri)
RELATIVE_PWD = $$system(realpath --canonicalize-missing --relative-to $$OUT_PWD $$PWD)

libblight_libblight_protocol_h.target = include/libblight_protocol/libblight_protocol.h
for(h, HEADERS){
    libblight_libblight_protocol_h.depends += $${RELATIVE_PWD}/$${h}
}
libblight_libblight_protocol_h.commands = \
    mkdir -p include/libblight_protocol && \
    echo $$HEADERS | xargs -rn1 | xargs -rI {} cp $$PWD/{} include/libblight_protocol/ && \
    echo $$DBUS_INTERFACES | xargs -rn1 | xargs -rI {} basename \"{}\" .xml | xargs -rI {} cp $$OUT_PWD/\"{}\"_interface.h include/libblight_protocol/

libblight_protocol_h.target = include/libblight_protocol.h
libblight_protocol_h.depends = libblight_libblight_protocol_h
libblight_protocol_h.commands = \
    echo \\$$LITERAL_HASH"pragma once" > include/libblight_protocol.h && \
    echo \"$$LITERAL_HASH"include \\\"libblight_protocol/libblight_protocol.h\\\"\"" >> include/libblight_protocol.h

libblight_protocol_h_install.files = \
    include/libblight_protocol.h \
    include/libblight_protocol
libblight_protocol_h_install.depends = libblight_protocol_h
libblight_protocol_h_install.path = /opt/include/
INSTALLS += libblight_protocol_h_install

QMAKE_EXTRA_TARGETS += libblight_libblight_protocol_h libblight_protocol_h libblight_protocol_h_install
POST_TARGETDEPS += $$libblight_libblight_protocol_h.target $$libblight_protocol_h.target
QMAKE_CLEAN += $$libblight_protocol_h.target include/libblight_protocol/*.h

TARGET = blight_protocol
target.path = /opt/lib
INSTALLS += target

QMAKE_PKGCONFIG_NAME = libblight_protocol
QMAKE_PKGCONFIG_DESCRIPTION = Shared library the Oxide\'s screen compositor protocol
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_PREFIX = /opt
QMAKE_PKGCONFIG_LIBDIR = /opt/lib
QMAKE_PKGCONFIG_INCDIR = /opt/include
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

DISTFILES += \
    Doxyfile
