linux-oe-g++{
    LIBS += -L$$OUT_PWD/../../shared/epaper -L$$(SYSROOT)/usr/lib/plugins/scenegraph -L$$(SYSROOT)/usr/plugins/scenegraph -lqsgepaper
    INCLUDEPATH += $$OUT_PWD/../../shared/epaper
    DEFINES += EPAPER
}
