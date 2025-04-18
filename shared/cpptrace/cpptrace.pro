contains(DEFINES, SENTRY){
    error("Configured to build sentry instead of cpptrace")
}
TEMPLATE = aux

PRE_TARGETDEPS += $$OUT_PWD/src/Makefile
cpptrace_makefile.target = $$OUT_PWD/src/Makefile
cpptrace_makefile.commands = \
    cmake -B $$OUT_PWD/src \
        -S $$PWD/src \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
QMAKE_EXTRA_TARGETS += cpptrace_makefile

PRE_TARGETDEPS += $$OUT_PWD/src/libcpptrace.so
cpptrace_build.target = $$OUT_PWD/src/libcpptrace.so
cpptrace_build.depends = cpptrace_makefile
cpptrace_build.commands = \
    cmake --build $$OUT_PWD/src --parallel
QMAKE_EXTRA_TARGETS += cpptrace_build

PRE_TARGETDEPS += $$OUT_PWD/lib/libcpptrace.so
cpptrace_install.target = $$OUT_PWD/lib/libcpptrace.so
cpptrace_install.depends = cpptrace_build
cpptrace_install.commands = \
    cmake --install $$OUT_PWD/src \
        --prefix $$OUT_PWD \
        --config RelWithDebInfo
QMAKE_EXTRA_TARGETS += cpptrace_install

QMAKE_CLEAN += -r $$OUT_PWD/src/

target.files = $$OUT_PWD/lib/libcpptrace.so
target.depends = cpptrace_build
target.path = /opt/lib/
INSTALLS += target
