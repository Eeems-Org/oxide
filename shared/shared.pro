TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    qpa

qpa.depends = liboxide
INSTALLS += $$SUBDIRS
