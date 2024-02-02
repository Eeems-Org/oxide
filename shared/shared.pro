TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    libblight \
    libblight_client \
    qpa

liboxide.depends = libblight
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
