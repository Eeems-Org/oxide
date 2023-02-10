TEMPLATE = subdirs

SUBDIRS = \
    shared \
    applications

applications.depends = shared

INSTALLS += $$SUBDIRS
