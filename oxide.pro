TEMPLATE = subdirs

SUBDIRS = \
    shared \
    applications \
    tests

applications.depends = shared
tests.depends = shared

INSTALLS += $$SUBDIRS
