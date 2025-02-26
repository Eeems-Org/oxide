!linux-oe-g++{
    error("epaper is only supported on the reMarkable")
}
TEMPLATE = aux

epframebuffer_h.target = raw_copy
epframebuffer_h.commands = cp $$PWD/epframebuffer.h $$PWD/libqsgepaper.so $$OUT_PWD
QMAKE_EXTRA_TARGETS += epframebuffer_h

linux-oe-g++{
    epframebuffer_h_install.files = epframebuffer.h libqsgepaper.so
    epframebuffer_h_install.path = /opt/include
    INSTALLS += epframebuffer_h_install
}

PRE_TARGETDEPS += $$epframebuffer_h.target
QMAKE_CLEAN += $$epframebuffer_h.target

