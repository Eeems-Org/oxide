TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    libblight_protocol \
    libblight \
    libblight_client \
    qpa \
    lockscreen_hook

liboxide.depends = libblight
libblight.depends = libblight_protocol
libblight_client.depends = libblight
qpa.depends = libblight liboxide
SUBDIRS += cpptrace
liboxide.depends += cpptrace
linux-oe-g++{
    SUBDIRS += epaper
    liboxide.depends += epaper
}
INSTALLS += $$SUBDIRS
