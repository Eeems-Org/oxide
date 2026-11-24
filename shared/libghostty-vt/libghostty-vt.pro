TEMPLATE = aux

include(../../qmake/common.pri)

ZIG_TARGET = $$getenv(ZIG_TARGET)
isEmpty(ZIG_TARGET) {
    ZIG_TARGET = native
}

PRE_TARGETDEPS += $$OUT_PWD/lib/libghostty-vt.so*
ghostty_vt_build.target = $$OUT_PWD/lib/libghostty-vt.so*
ghostty_vt_build.commands = \
    cd $$PWD/src && \
    zig build \
        -Demit-lib-vt \
        -Dtarget=$$ZIG_TARGET \
        -Doptimize=ReleaseFast \
        --prefix $$ROOT_INSTALL_PATH && \
    zig build install \
        -Demit-lib-vt \
        -Dtarget=$$ZIG_TARGET \
        -Doptimize=ReleaseFast \
        --prefix $$OUT_PWD && \
    rm -f $$OUT_PWD/lib/libghostty-vt.a
QMAKE_EXTRA_TARGETS += ghostty_vt_build

QMAKE_CLEAN += -r $$OUT_PWD/lib $$OUT_PWD/include

target.files = $$OUT_PWD/lib/libghostty-vt.so*
target.depends = ghostty_vt_build
target.path = $$LIB_INSTALL_PATH/
INSTALLS += target

pkgconfig.files = $$OUT_PWD/lib/pkgconfig/libghostty-vt.pc
pkgconfig.depends = ghostty_vt_build
pkgconfig.path = $$LIB_INSTALL_PATH/pkgconfig
INSTALLS += pkgconfig

cmake.files = $$OUT_PWD/lib/cmake/ghostty-vt/*
cmake.depends = ghostty_vt_build
cmake.path = $$LIB_INSTALL_PATH/cmake/ghostty-vt
INSTALLS += cmake

headers.files = $$OUT_PWD/include/ghostty/*
headers.depends = ghostty_vt_build
headers.path = $$LIB_INSTALL_PATH/../include/ghostty
INSTALLS += headers
