TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    qpa \
    preload

qpa.depends = liboxide
preload.depends = liboxide
INSTALLS += $$SUBDIRS
