TEMPLATE = subdirs

SUBDIRS = liboxide
contains(DEFINES, SENTRY){
    SUBDIRS += sentry
    liboxide.depends += sentry
}
linux-oe-g++{
    SUBDIRS += epaper
    liboxide.depends += epaper
}
INSTALLS += $$SUBDIRS
