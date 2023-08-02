#include "oxideservices.h"

OxideServices::OxideServices() : QPlatformServices(){}

bool OxideServices::openUrl(const QUrl& url){
    Q_UNUSED(url)
    // TODO - use xdg-open
    return false;
}

bool OxideServices::openDocument(const QUrl& url){
    Q_UNUSED(url)
    // TODO - use xdg-open
    return false;
}

QByteArray OxideServices::desktopEnvironment() const{ return qgetenv("XDG_CURRENT_DESKTOP"); }
