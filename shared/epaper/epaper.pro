!linux-oe-g++{
    error("epaper is only supported on the reMarkable")
}
TEMPLATE = aux

include(../../qmake/common.pri)

epframebuffer_h.target = raw_copy
epframebuffer_h.commands = cp $$PWD/epframebuffer.h $$OUT_PWD
QMAKE_EXTRA_TARGETS += epframebuffer_h

linux-oe-g++{
    epframebuffer_h_install.files = epframebuffer.h
    epframebuffer_h_install.path = $$INCLUDE_INSTALL_PATH
    INSTALLS += epframebuffer_h_install
}

PRE_TARGETDEPS += $$epframebuffer_h.target
QMAKE_CLEAN += $$epframebuffer_h.target

