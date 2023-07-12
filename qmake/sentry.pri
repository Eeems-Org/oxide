contains(DEFINES, SENTRY){
    LIBSENTRY_ROOT = $$OUT_PWD/../../shared/sentry
    LIBSENTRY_SRC = $$LIBSENTRY_ROOT/src
    LIBSENTRY_LIB = $$LIBSENTRY_ROOT/lib
    LIBSENTRY_SO = $$LIBSENTRY_LIB/libsentry.so
    LIBSENTRY_INC = $$LIBSENTRY_ROOT/include
    LIBSENTRY_H = $$LIBSENTRY_INC/sentry.h
    LIBS_PRIVATE += -L$$LIBSENTRY_LIB -lsentry -ldl -lcurl -lbreakpad_client
    INCLUDEPATH += $$LIBSENTRY_INC
    DEPENDPATH += $$LIBSENTRY_LIB

    PRE_TARGETDEPS += $$LIBSENTRY_SRC/Makefile
    sentry_makefile.target = $$LIBSENTRY_SRC/Makefile
    sentry_makefile.commands = cd $$_PRO_FILE_PWD_/../../shared/sentry && \
        cmake -B $$LIBSENTRY_SRC \
            -DBUILD_SHARED_LIBS=ON \
            -DSENTRY_INTEGRATION_QT=ON \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DSENTRY_PIC=OFF \
            -DSENTRY_BACKEND=breakpad \
            -DSENTRY_BREAKPAD_SYSTEM=OFF \
            -DSENTRY_EXPORT_SYMBOLS=ON \
            -DSENTRY_PTHREAD=ON
    QMAKE_EXTRA_TARGETS += sentry_makefile

    PRE_TARGETDEPS += $$LIBSENTRY_SRC/libsentry.so
    sentry_build.target = $$LIBSENTRY_SRC/libsentry.so
    sentry_build.path = /opt/lib/
    sentry_build.depends = sentry_makefile
    sentry_build.commands = cd $$_PRO_FILE_PWD_/../../shared/sentry && \
        cmake --build $$LIBSENTRY_SRC --parallel
    QMAKE_EXTRA_TARGETS += sentry_build

    PRE_TARGETDEPS += $$LIBSENTRY_SO
    sentry_install.target = $$LIBSENTRY_SO
    sentry_install.path = /opt/lib/
    sentry_install.depends = sentry_build
    sentry_install.commands = cd $$_PRO_FILE_PWD_/../../shared/sentry && \
        cmake --install $$LIBSENTRY_SRC --prefix $$LIBSENTRY_ROOT --config RelWithDebInfo
    QMAKE_EXTRA_TARGETS += sentry_install
}
