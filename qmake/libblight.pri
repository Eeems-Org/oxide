contains(DEFINES, LIBBLIGHT_PRIVATE){
    LIBS_PRIVATE += -L$$OUT_PWD/../../shared/libblight -lblight
}else{
    LIBS += -L$$OUT_PWD/../../shared/libblight -lblight
}
INCLUDEPATH += $$OUT_PWD/../../shared/libblight/include

linux-oe-g++{
    DEFINES += EPAPER
}
include(libblight_protocol.pri)
