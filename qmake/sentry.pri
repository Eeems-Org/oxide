linux-oe-g++{
    contains(DEFINES, SENTRY){
        LIBSENTRY_ROOT = $$OUT_PWD/../../shared/sentry
        LIBSENTRY_LIB = $$LIBSENTRY_ROOT/lib
        LIBSENTRY_INC = $$LIBSENTRY_ROOT/include
        LIBS_PRIVATE += -L$$LIBSENTRY_LIB -lsentry -ldl -lcurl -lbreakpad_client
        INCLUDEPATH += $$LIBSENTRY_INC
        DEPENDPATH += $$LIBSENTRY_LIB
    }
}
