!linux-oe-g++{
    error("epaper is only supported on the reMarkable")
}
TEMPLATE = aux

include(../../qmake/common.pri)

VERSION = 1.0

DISTFILES = \
    epframebuffer.h

TARGET = qsgepaper
target.path = /opt/lib
INSTALLS += target


epframebuffer_h.target = epframebuffer.h
epframebuffer_h.commands = cp $$PWD/epframebuffer.h $$OUT_PWD
QMAKE_EXTRA_TARGETS += epframebuffer_h

linux-oe-g++{
    epframebuffer_h_install.files = epframebuffer.h libqsgepaper.so
    epframebuffer_h_install.path = /opt/include
    INSTALLS += epframebuffer_h_install
}

PRE_TARGETDEPS += $$epframebuffer_h.target
QMAKE_CLEAN += $$epframebuffer_h.target

