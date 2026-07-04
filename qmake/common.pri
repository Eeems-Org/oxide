VERSION = 3.0

!contains(DEFINES, QT_DEPRECATED_WARNINGS){
    DEFINES += QT_DEPRECATED_WARNINGS
}
isEmpty(QT_DISABLE_DEPRECATED_BEFORE){
    QT_DISABLE_DEPRECATED_BEFORE = 0x060000
}else{
    message("Using override deprecation value")
}

QT_CONFIG -= no-pkg-config
CONFIG += link_pkgconfig

DEFINES ~= s/QT_DISABLE_DEPRECATED_BEFORE=.+/
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=$${QT_DISABLE_DEPRECATED_BEFORE}
CONFIG(debug, debug|release){
    contains(DEFINES, SANITIZER){
        CONFIG += sanitizer
        CONFIG += sanitize_address
        CONFIG += sanitize_undefined
    }
}

linux-oe-g++{
    DEFINES += EPAPER
}
ROOT_INSTALL_PATH = /home/root/.vellum
LIB_INSTALL_PATH = $$ROOT_INSTALL_PATH/lib
INCLUDE_INSTALL_PATH = $$ROOT_INSTALL_PATH/include
BIN_INSTALL_PATH = $$ROOT_INSTALL_PATH/bin
SHARE_INSTALL_PATH = $$ROOT_INSTALL_PATH/share
CONFIG_INSTALL_PATH = /home/root/.config
APPLICATIONS_INSTALL_PATH = /home/root/.local/share/applications
ICONS_INSTALL_PATH = /home/root/.local/share/icons/oxide/48x48/apps
SPLASH_INSTALL_PATH = /home/root/.local/share/icons/oxide/702x702/splash
TESTS_INSTALL_PATH = /home/root/.local/share/tests

QMAKE_RPATHDIR += \
    /lib \
    /usr/lib \
    $$LIB_INSTALL_PATH
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += SPLASH_INSTALL_PATH=\\\"$$SPLASH_INSTALL_PATH\\\"
DEFINES += ICONS_INSTALL_PATH=\\\"$$ICONS_INSTALL_PATH\\\"

CONFIG += c++17
CONFIG += c++20
# CONFIG += c++latest

CONFIG(release, debug|release){
    !contains(DEFINES, DISABLE_LTO){
        CONFIG += ltcg
        QMAKE_LFLAGS += -flto
    }
}

!contains(DEFINES, DISABLE_PIC){
    QMAKE_CFLAGS += -fPIC
    QMAKE_CXXFLAGS += -fPIC
}

