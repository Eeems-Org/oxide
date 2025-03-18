TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    libblight_protocol \
    libblight \
    libblight_client \
    qpa

liboxide.depends = libblight
libblight.depends = libblight_protocol
libblight_client.depends = libblight
qpa.depends = libblight liboxide
contains(DEFINES, SENTRY){
    SUBDIRS += sentry
    liboxide.depends += sentry
}
linux-oe-g++{
    SUBDIRS += epaper
    liboxide.depends += epaper
}
INSTALLS += $$SUBDIRS
