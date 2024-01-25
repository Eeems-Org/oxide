#pragma once

#include <QObject>

#include "private/qdevicediscovery_p.h"
#include <private/qinputdevicemanager_p_p.h>
#include <QtInputSupport/private/devicehandlerlist_p.h>

#include "oxideeventhandler.h"

class OxideEventManager : public QObject {
    Q_OBJECT
public:
    OxideEventManager();

private slots:
    void deviceDetected(QInputDeviceManager::DeviceType type, const QString& device);
    void deviceRemoved(QInputDeviceManager::DeviceType type, const QString& device);
    void deviceListChanged(QInputDeviceManager::DeviceType type);

private:
    void setup(
        QDeviceDiscovery::QDeviceType privateType,
        QInputDeviceManager::DeviceType publicType
    );
    QMap<QInputDeviceManager::DeviceType, QStringList> m_devices;
    OxideEventHandler m_handler;
};
