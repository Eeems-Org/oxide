TEMPLATE = subdirs

SUBDIRS = \
    liboxide \
    epaper

liboxide.depends = epaper
epaper.file = epaper/epaper-qpa.pro

INSTALLS += $$SUBDIRS
