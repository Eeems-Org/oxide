TEMPLATE = subdirs

SUBDIRS =  \
    libblight \
    liboxide \
    libblight_protocol \
    test_app

INSTALLS += $$SUBDIRS
