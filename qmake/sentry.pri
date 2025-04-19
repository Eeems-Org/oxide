linux-oe-g++{
    contains(DEFINES, SENTRY){
        LIBSENTRY_ROOT = $$OUT_PWD/../../shared/sentry
        LIBSENTRY_LIB = $$LIBSENTRY_ROOT/lib
        LIBSENTRY_INC = $$LIBSENTRY_ROOT/include
        LIBS_PRIVATE += -L$$LIBSENTRY_LIB -lsentry -ldl -lcurl -lbreakpad_client
        INCLUDEPATH += $$LIBSENTRY_INC
        DEPENDPATH += $$LIBSENTRY_LIB
    }else{
        CONFIG(debug, debug|release){
            CPPTRACE_ROOT = $$OUT_PWD/../../shared/cpptrace
            CPPTRACE_LIB = $$CPPTRACE_ROOT/lib
            CPPTRACE_INC = $$CPPTRACE_ROOT/include
            LIBS += -L$$CPPTRACE_LIB -lcpptrace -ldwarf -lz -ldl
            INCLUDEPATH += $$CPPTRACE_INC
            DEPENDPATH += $$CPPTRACE_LIB
            QMAKE_LFLAGS += -rdynamic
        }
    }
}
