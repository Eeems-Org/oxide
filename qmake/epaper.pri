linux-oe-g++{
    LIBS += -L$$OUT_PWD/../../shared/epaper -lqsgepaper
    INCLUDEPATH += $$OUT_PWD/../../shared/epaper
    DEFINES += EPAPER
}
