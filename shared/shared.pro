TEMPLATE = subdirs

SUBDIRS = \
    cpptrace \
    libblight \
    libblight_protocol \
    liboxide \
    qpa \
    sentry

liboxide.depends = \
    cpptrace \
    libblight \
    sentry
libblight.depends = libblight_protocol
qpa.depends = libblight liboxide
linux-oe-g++{
    SUBDIRS += \
        epaper \
        libblight_client \
        lockscreen_hook
    liboxide.depends += epaper
    libblight_client.depends = libblight
}
INSTALLS += $$SUBDIRS
