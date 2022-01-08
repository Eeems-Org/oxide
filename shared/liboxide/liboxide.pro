QT -= gui

TEMPLATE = lib
DEFINES += LIBOXIDE_LIBRARY

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    liboxide.cpp

HEADERS += \
    liboxide_global.h \
    liboxide.h

INCLUDEPATH += ../../shared
LIBS += -L$$PWD/../../shared/ -lqsgepaper
INCLUDEPATH += $$PWD/../../shared
DEPENDPATH += $$PWD/../../shared

target.path = /opt/usr/lib
!isEmpty(target.path): INSTALLS += target
