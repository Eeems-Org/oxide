TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    fb2png.cpp

TARGET=button-capture

target.path = /opt/bin
INSTALLS += target

HEADERS += \
    fb2png.h \
    inputmanager.h

linux-oe-g++ {
    LIBS += -lpng16
}
