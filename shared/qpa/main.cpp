#include <qpa/qplatformintegrationplugin.h>
#include "oxideintegration.h"

QT_BEGIN_NAMESPACE


class OxideIntegrationPlugin : public QPlatformIntegrationPlugin{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "oxide.json")

public:
    QPlatformIntegration* create(const QString&, const QStringList&) override;
};

QPlatformIntegration* OxideIntegrationPlugin::create(const QString& system, const QStringList& paramList){
    if(!system.compare(QLatin1String("oxide"), Qt::CaseInsensitive)){
        return new OxideIntegration(paramList);
    }
    return nullptr;
}

QT_END_NAMESPACE

#include "main.moc"
