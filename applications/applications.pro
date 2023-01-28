TEMPLATE = subdirs

SUBDIRS = \
    launcher \
    lockscreen \
    notify-send \
    process-manager \
    screenshot-tool \
    screenshot-viewer \
    settings-manager \
    system-service \
    task-switcher

launcher.depends = system-service
lockscreen.depends = system-service
notify-send.depends = system-service
process-manager.depends =
screenshot-tool.depends = system-service
screenshot-viewer.depends = system-service
settings-manager.depends = system-service
system-service.depends =
task-switcher.depends = system-service

INSTALLS += \
    launcher \
    lockscreen \
    notify-send \
    process-manager \
    screenshot-tool \
    screenshot-viewer \
    settings-manager \
    system-service \
    task-switcher
