VERSION = 3.0

!contains(DEFINES, QT_DEPRECATED_WARNINGS){
    DEFINES += QT_DEPRECATED_WARNINGS
}
isEmpty(QT_DISABLE_DEPRECATED_BEFORE){
    QT_DISABLE_DEPRECATED_BEFORE = 0x060000
}else{
    message("Using override deprecation value")
}
DEFINES ~= s/QT_DISABLE_DEPRECATED_BEFORE=.+/
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=$${QT_DISABLE_DEPRECATED_BEFORE}
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

linux-oe-g++{
    DEFINES += EPAPER
}

QMAKE_RPATHDIR += /lib /usr/lib /opt/lib /opt/usr/lib
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

CONFIG += c++17
CONFIG += c++20
CONFIG += c++latest

!contains(DEFINES, DISABLE_LTO){
    CONFIG += ltcg
    QMAKE_LFLAGS += -flto
}

!contains(DEFINES, DISABLE_PIC){
    QMAKE_CFLAGS += -fPIC
    QMAKE_CXXFLAGS += -fPIC
}

QT_CONFIG -= no-pkg-config
CONFIG += link_pkgconfig
