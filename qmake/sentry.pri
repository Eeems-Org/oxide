contains(DEFINES, SENTRY){
    LIBSENTRY_ROOT = $$PWD/../.build/sentry
    LIBSENTRY_LIB = $$LIBSENTRY_ROOT/lib
    LIBSENTRY_SO = $$LIBSENTRY_LIB/libsentry.so
    !exists($$LIBSENTRY_SO){
        error(Missing $$LIBSENTRY_SO)
    }
    LIBSENTRY_INC = $$LIBSENTRY_ROOT/include
    LIBSENTRY_H = $$LIBSENTRY_INC/sentry.h
    !exists($$LIBSENTRY_H){
        error(Missing $$LIBSENTRY_H)
    }
    LIBS_PRIVATE += -L$$LIBSENTRY_LIB -lsentry -ldl -lcurl -lbreakpad_client
    INCLUDEPATH += $$LIBSENTRY_INC
    DEPENDPATH += $$LIBSENTRY_LIB
}
