#pragma once

#include <qpa/qplatformservices.h>

class OxideServices : QPlatformServices{
public:
    OxideServices();
    bool openUrl(const QUrl& url) override;
    bool openDocument(const QUrl& url) override;
    QByteArray desktopEnvironment() const override;
};
