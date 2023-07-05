TEMPLATE = subdirs

SUBDIRS = \
    desktop-file-edit \
    desktop-file-install \
    desktop-file-validate \
    gio \
    inject_evdev \
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
    xdg-desktop-menu \
    xdg-icon-resource \
    xdg-open \
    xdg-settings \
    ui_test

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
xdg-open.depends = system-service
gio.depends = system-service
xdg-settings.depends = system-service
xdg-icon-resource.depends = system-service
desktop-file-edit.depends = desktop-file-edit
desktop-file-install.depends =
inject_evdev.depends =
ui_test.depends = system-service

INSTALLS += $$SUBDIRS
