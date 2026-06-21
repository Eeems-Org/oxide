TEMPLATE = aux

include(../../qmake/common.pri)

launcherctl.target = raw_copy
launcherctl.commands = cp $$PWD/launcherctl $$PWD/none $$OUT_PWD
QMAKE_EXTRA_TARGETS += launcherctl

launcherctl_install.files = launcherctl
launcherctl_install.path = $$BIN_INSTALL_PATH
INSTALLS += launcherctl_install

none_install.files = none
none_install.path = $$SHARE_INSTALL_PATH/launcherctl
INSTALLS += none_install

hooks_install.files = \
    unlock \
    success
hooks_install.path = /home/root/.local/share/lockscreen_hook.d
INSTALLS += hooks_install

PRE_TARGETDEPS += $$launcherctl.target
QMAKE_CLEAN += $$launcherctl.target

DISTFILES += \
    launcherctl \
    none
