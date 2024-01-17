VERSION = 2.8

!contains(DEFINES, QT_DEPRECATED_WARNINGS){
    DEFINES += QT_DEPRECATED_WARNINGS
}
!contains(DEFINES, QT_DISABLE_DEPRECATED_BEFORE){
    isEmpty(QT_DISABLE_DEPRECATED_BEFORE){
        DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000
    }else{
        DEFINES += QT_DISABLE_DEPRECATED_BEFORE=$${QT_DISABLE_DEPRECATED_BEFORE}
    }
}
CONFIG(debug, debug|release){
    LIBS += -lunwind
    contains(DEFINES, SANITIZER){
        QMAKE_LFLAGS += -fno-omit-frame-pointer
        QMAKE_LFLAGS += -fsanitize-recover=address

        QMAKE_LFLAGS += -fsanitize=address
        QMAKE_LFLAGS += -fsanitize=leak
        # QMAKE_LFLAGS += -fsanitize=thread # Incompatible with address and leak
        QMAKE_LFLAGS += -fsanitize=undefined
        QMAKE_LFLAGS += -fsanitize=pointer-compare
        QMAKE_LFLAGS += -fsanitize=pointer-subtract
    }
}

QMAKE_RPATHDIR += /lib /usr/lib /opt/lib /opt/usr/lib
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

CONFIG += ltcg
CONFIG += c++17
CONFIG += c++20
CONFIG += c++latest

QMAKE_LFLAGS += -flto
QMAKE_CFLAGS += -fPIC
QMAKE_CXXFLAGS += -fPIC
