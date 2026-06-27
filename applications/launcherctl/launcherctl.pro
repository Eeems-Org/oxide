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
    current-launcher.h \
    icommand.h \
    is-active-launcher.h \
    is-current-launcher.h \
    is-enabled-launcher.h \
    list-apps.h \
    list-launchers.h \
    list-paused-apps.h \
    list-running-apps.h \
    logs.h \
    pause-app.h \
    restart-launcher.h \
    resume-app.h \
    start-app.h \
    start-launcher.h \
    status.h \
    stop-app.h \
    stop-launcher.h \
    switch-launcher.h

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
