TARGET = oxide
TEMPLATE = lib

include(../../qmake/common.pri)

CONFIG += plugin
CONFIG += qpa/genericunixfontdatabase

QT += core-private
QT += core
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
          oxideeventfilter.cpp \
          oxideeventhandler.cpp \
          oxideintegration.cpp \
          oxidescreen.cpp \
          oxideservices.cpp \
          oxidewindow.cpp
HEADERS = oxidebackingstore.h \
          oxideeventfilter.h \
          oxideeventhandler.h \
          oxideintegration.h \
          oxidescreen.h \
          oxideservices.h \
          oxidewindow.h

OTHER_FILES += oxide.json

target.path += /opt/usr/lib/plugins/platforms
INSTALLS += target

DISTFILES += \
    oxide.json

DEFINES += LIBOXIDE_PRIVATE
include(../../qmake/liboxide.pri)
