VERSION = 3.0

DEFINES += QT_DEPRECATED_WARNINGS
!contains(DEFINES, QT_DISABLE_DEPRECATED_BEFORE){
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x051510
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
