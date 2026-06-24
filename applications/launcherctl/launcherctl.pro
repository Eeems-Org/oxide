QT -= gui
QT += dbus

CONFIG += c++17 console
CONFIG -= app_bundle

SOURCES += \
    common.cpp \
    icommand.cpp \
    main.cpp

HEADERS += \
    active-launcher.h \
    common.h \
    icommand.h \
    status.h \
    current-launcher.h \
    list-launchers.h \
    switch-launcher.h \
    start-launcher.h \
    stop-launcher.h \
    list-apps.h \
    list-running-apps.h \
    list-paused-apps.h \
    start-app.h \
    stop-app.h \
    pause-app.h \
    resume-app.h \
    is-current-launcher.h \
    is-enabled-launcher.h \
    is-active-launcher.h \
    logs.h

TARGET = launcherctl
include(../../qmake/common.pri)
target.path = $$BIN_INSTALL_PATH
INSTALLS += target

include(../../qmake/liboxide.pri)

none_install.files = none
none_install.path = $$SHARE_INSTALL_PATH/launcherctl
INSTALLS += none_install

hooks_install.files = \
    unlock \
    success
hooks_install.path = /home/root/.local/share/lockscreen_hook.d
INSTALLS += hooks_install

DISTFILES += \
    none \
    unlock \
    success
