TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    qpa \
    preload \
    preload-qt \
    epaper

liboxide.depends = epaper
qpa.depends = liboxide
preload.depends = liboxide
preload-qt.depends = liboxide

contains(DEFINES, SENTRY){
    SUBDIRS += sentry
    liboxide.depends += sentry
    qpa.depends += sentry
    preload.depends += sentry
    preload-qt.depends += sentry
}

INSTALLS += $$SUBDIRS
