TEMPLATE = aux

include(../../qmake/common.pri)

ZIG_TARGET = $$getenv(ZIG_TARGET)
isEmpty(ZIG_TARGET) {
    ZIG_TARGET = native
}

PRE_TARGETDEPS += $$OUT_PWD/lib/libghostty-vt.so.1.0.1
ghostty_vt_build.target = $$OUT_PWD/lib/libghostty-vt.so.1.0.1
ghostty_vt_build.commands = \
    cd $$PWD/src && \
    ZIG_GLOBAL_CACHE_DIR=/tmp/oxide-zig-cache zig build -Demit-lib-vt \
        -Dtarget=$$ZIG_TARGET \
        -Doptimize=ReleaseFast \
        -Dstrip=true \
        --cache-dir /tmp/oxide-zig-cache \
        --prefix $$OUT_PWD
QMAKE_EXTRA_TARGETS += ghostty_vt_build

QMAKE_CLEAN += -r $$OUT_PWD/lib $$OUT_PWD/include

target.files = $$OUT_PWD/lib/libghostty-vt.so $$OUT_PWD/lib/libghostty-vt.so.1 $$OUT_PWD/lib/libghostty-vt.so.1.0.1
target.depends = ghostty_vt_build
target.path = $$LIB_INSTALL_PATH/
INSTALLS += target
