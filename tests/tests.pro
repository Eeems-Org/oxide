TEMPLATE = subdirs

SUBDIRS =  \
    libblight \
    liboxide \
    libblight_protocol \
    test_app \
    desktop-file-edit

INSTALLS += $$SUBDIRS
