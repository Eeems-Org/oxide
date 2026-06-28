TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    libblight_protocol \
    libblight \
    libblight_client \
    qpa \
    lockscreen_hook \
    cpptrace \
    sentry

liboxide.depends = \
    libblight \
    cpptrace \
    sentry
libblight.depends = libblight_protocol
libblight_client.depends = libblight
qpa.depends = libblight liboxide
linux-oe-g++{
    SUBDIRS += epaper
    liboxide.depends += epaper
}
INSTALLS += $$SUBDIRS
