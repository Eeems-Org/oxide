TEMPLATE = subdirs

SUBDIRS = \
    desktop-file-validate \
    launcher \
    lockscreen \
    notify-send \
    process-manager \
    screenshot-tool \
    screenshot-viewer \
    settings-manager \
    system-service \
    task-switcher \
    update-desktop-database \
    xdg-desktop-icon \
    xdg-desktop-menu

launcher.depends = system-service update-desktop-database
lockscreen.depends = system-service
notify-send.depends = system-service
process-manager.depends =
screenshot-tool.depends = system-service
screenshot-viewer.depends = system-service
settings-manager.depends = system-service
system-service.depends =
task-switcher.depends = system-service
update-desktop-database.depends = system-service
xdg-desktop-icon.depends = system-service
xdg-desktop-menu.depends = system-service

INSTALLS += $$SUBDIRS
