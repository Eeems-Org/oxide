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
INSTALLS += $$SUBDIRS
