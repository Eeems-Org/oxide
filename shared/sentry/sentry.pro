TEMPLATE = aux

PRE_TARGETDEPS += $$OUT_PWD/src/Makefile
sentry_makefile.target = $$OUT_PWD/src/Makefile
sentry_makefile.commands = \
    cmake -B $$OUT_PWD/src \
        -S $$PWD/src \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBUILD_SHARED_LIBS=ON \
        -DSENTRY_INTEGRATION_QT=ON \
        -DSENTRY_TRANSPORT=curl \
        -DSENTRY_BACKEND=breakpad \
        -DSENTRY_BREAKPAD_SYSTEM=OFF \
        -DSENTRY_SDK_NAME=sentry.native
QMAKE_EXTRA_TARGETS += sentry_makefile

PRE_TARGETDEPS += $$OUT_PWD/src/libsentry.so
sentry_build.target = $$OUT_PWD/src/libsentry.so
sentry_build.depends = sentry_makefile
sentry_build.commands = \
    cmake --build $$OUT_PWD/src --parallel
QMAKE_EXTRA_TARGETS += sentry_build

PRE_TARGETDEPS += $$OUT_PWD/lib/libsentry.so
sentry_install.target = $$OUT_PWD/lib/libsentry.so
sentry_install.depends = sentry_build
sentry_install.commands = \
    cmake --install $$OUT_PWD/src --prefix $$OUT_PWD --config RelWithDebInfo
QMAKE_EXTRA_TARGETS += sentry_install

QMAKE_CLEAN += -r $$OUT_PWD/src/

target.files = $$OUT_PWD/lib/libsentry.so
target.depends = sentry_install
target.path = /opt/lib/
INSTALLS += target
