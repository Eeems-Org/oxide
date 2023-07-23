TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    qpa \
    preload \
    preload-qt

qpa.depends = liboxide
preload.depends = liboxide
preload-qt.depends = liboxide
INSTALLS += $$SUBDIRS
