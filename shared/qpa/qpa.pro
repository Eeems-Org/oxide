TARGET = oxide
TEMPLATE = lib

CONFIG += plugin
CONFIG += qpa/genericunixfontdatabase c++17
QT += core-private gui-private input_support-private
QT += dbus

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
          oxidewindow.cpp
HEADERS = oxidebackingstore.h \
          oxideintegration.h \
          oxidescreen.h \
          oxidewindow.h

OTHER_FILES += oxide.json

target.path += /opt/usr/lib/plugins/platforms
INSTALLS += target

DISTFILES += \
    oxide.json

include(../../qmake/liboxide.pri)
include(../../qmake/sentry.pri)
