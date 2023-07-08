TARGET = oxide
TEMPLATE = lib

CONFIG += plugin
CONFIG += qpa/genericunixfontdatabase
CONFIG += c++17
QT += core-private
QT += gui-private
QT += input_support-private

qtHaveModule(fontdatabase_support-private) {
    QT += fontdatabase_support-private 
}
qtHaveModule(eventdispatcher_support-private) {
    QT += eventdispatcher_support-private
}

SOURCES = main.cpp \
          oxidebackingstore.cpp \
          oxideintegration.cpp \
          oxidescreen.cpp \
          oxidetabletdata.cpp \
          oxidetouchscreendata.cpp \
          oxidewindow.cpp
HEADERS = oxidebackingstore.h \
          oxideintegration.h \
          oxidescreen.h \
          oxidetabletdata.h \
          oxidetouchscreendata.h \
          oxidewindow.h

OTHER_FILES += oxide.json

target.path += /opt/usr/lib/plugins/platforms
INSTALLS += target

DISTFILES += \
    oxide.json

include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
