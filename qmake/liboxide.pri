contains(DEFINES, LIBOXIDE_PRIVATE){
    LIBS_PRIVATE += -L$$OUT_PWD/../../shared/liboxide -loxide
}else{
    LIBS += -L$$OUT_PWD/../../shared/liboxide -loxide
}
INCLUDEPATH += $$OUT_PWD/../../shared/liboxide/include
QT += network
QT += dbus
include(sentry.pri)
