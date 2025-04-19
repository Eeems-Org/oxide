contains(DEFINES, LIBBLIGHT_PROTOCOL_PRIVATE){
    LIBS_PRIVATE += -L$$OUT_PWD/../../shared/libblight_protocol -lblight_protocol
}else{
    LIBS += -L$$OUT_PWD/../../shared/libblight_protocol -lblight_protocol
}
INCLUDEPATH += $$OUT_PWD/../../shared/libblight_protocol/include

linux-oe-g++{
    DEFINES += EPAPER
}
