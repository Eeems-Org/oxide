TEMPLATE = subdirs

SUBDIRS = \
    sentry \
    liboxide \
    epaper

liboxide.depends = sentry epaper
INSTALLS += $$SUBDIRS
