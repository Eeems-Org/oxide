linux-oe-g++{
    isEmpty(SYSROOT){
        SYSROOT = $$(SYSROOT)
    }
    isEmpty(SYSROOT){
        SYSROOT = $$(OECORE_TARGET_SYSROOT)
    }
    isEmpty(SYSROOT){
        SYSROOT = $$(SDKTARGETSYSROOT)
    }
    isEmpty(SYSROOT){
        SYSROOT = $$(PKG_CONFIG_SYSROOT_DIR)
    }
    isEmpty(SYSROOT){
        error("SYSROOT not set")
    }
    LIBS += -L$$OUT_PWD/../../shared/epaper
    LIBS += -L$$SYSROOT/usr/lib/plugins/scenegraph
    LIBS += -L$$SYSROOT/usr/plugins/scenegraph
    LIBS += -lqsgepaper
    INCLUDEPATH += $$OUT_PWD/../../shared/epaper
    DEFINES += EPAPER
}
