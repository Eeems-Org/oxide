TEMPLATE = subdirs

SUBDIRS = \
    desktop-file-edit \
    desktop-file-install \
    desktop-file-validate \
    display-server \
    fbinfo \
    file-manager \
    gio \
    image-viewer \
    inject_evdev \
    launcher \
    launcherctl \
    lockscreen \
    notify-send \
    openwith \
    process-manager \
    screenshot-viewer \
    settings \
    settings-manager \
    sketchpad \
    system-service \
    task-switcher \
    terminal \
    text-viewer \
    topbar \
    update-desktop-database \
    xclip \
    xdg-desktop-icon \
    xdg-desktop-menu \
    xdg-icon-resource \
    xdg-mime \
    xdg-open \
    xdg-settings

desktop-file-edit.depends =
desktop-file-install.depends =
display-server.depends = system-service
fbinfo.depends =
file-manager.depends = system-service
gio.depends = system-service
image-viewer.depends =
inject_evdev.depends =
launcher.depends = system-service update-desktop-database
launcherctl.depends = system-service
lockscreen.depends = system-service
notify-send.depends = system-service
openwith.depends = system-service
process-manager.depends =
screenshot-viewer.depends = system-service
settings-manager.depends = system-service
settings.depends = system-service
sketchpad.depends = system-service
system-service.depends =
task-switcher.depends = system-service
terminal.depends = system-service
text-viewer.depends =
topbar.depends = system-service
update-desktop-database.depends = system-service
xclip.depends = system-service
xdg-desktop-icon.depends = system-service
xdg-desktop-menu.depends = system-service
xdg-icon-resource.depends = system-service
xdg-mime.depends = system-service
xdg-open.depends = system-service
xdg-settings.depends = system-service

INSTALLS += $$SUBDIRS
