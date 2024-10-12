!linux-oe-g++{
    error("epaper is only supported on the reMarkable")
}
TEMPLATE = lib
QT += core
QT += quick

include(../../qmake/common.pri)

VERSION = 1.0

DISTFILES = \
    epframebuffer.h

TARGET = qsgepaper
target.path = /opt/lib
INSTALLS += target

libqsgepaper.target = libqsgepaper.a.in
libqsgepaper.commands = cp $$PWD/libqsgepaper.a $$OUT_PWD/$$libqsgepaper.target

QMAKE_EXTRA_TARGETS += libqsgepaper
PRE_TARGETDEPS += $$libqsgepaper.target
QMAKE_CLEAN += $$libqsgepaper.target

LIBS += -Wl,--whole-archive $$libqsgepaper.target -Wl,--no-whole-archive

epframebuffer_h.target = epframebuffer.h
epframebuffer_h.commands = cp $$PWD/epframebuffer.h $$OUT_PWD
QMAKE_EXTRA_TARGETS += epframebuffer_h

linux-oe-g++{
    epframebuffer_h_install.files = epframebuffer.h
    epframebuffer_h_install.path = /opt/include
    INSTALLS += epframebuffer_h_install
}

PRE_TARGETDEPS += $$epframebuffer_h.target $$epimagenode_h.target
QMAKE_CLEAN += $$epframebuffer_h.target $$epimagenode_h.target

