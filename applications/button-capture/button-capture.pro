TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp

TARGET=button-capture

target.path = /opt/bin
INSTALLS += target
