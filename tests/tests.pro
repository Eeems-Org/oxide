TEMPLATE = subdirs

SUBDIRS =  \
    libblight \
    liboxide \
    libblight_protocol \
    test_app \
    desktop_file_edit

INSTALLS += $$SUBDIRS
