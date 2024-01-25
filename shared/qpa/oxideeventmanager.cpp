#include "oxideeventmanager.h"

#include <QDebug>
#include <QFileInfo>

#include <private/qdevicediscovery_p.h>
#include <private/qguiapplication_p.h>
#include <private/qmemory_p.h>

OxideEventManager::OxideEventManager(const QStringList& parameters)
: QObject(),
  m_devices(),
  m_handler(this, parameters)
{
    setup(QDeviceDiscovery::Device_Keyboard, QInputDeviceManager::DeviceTypeKeyboard);
    setup(QDeviceDiscovery::Device_Tablet, QInputDeviceManager::DeviceTypeTablet);
    setup(QDeviceDiscovery::Device_Touchscreen, QInputDeviceManager::DeviceTypeTouch);
    setup(
        QDeviceDiscovery::Device_Mouse | QDeviceDiscovery::Device_Touchpad,
        QInputDeviceManager::DeviceTypePointer
    );
}

void OxideEventManager::setup(
    QDeviceDiscovery::QDeviceTypes privateTypes,
    QInputDeviceManager::DeviceType publicType
){
    if(auto deviceDiscovery = QDeviceDiscovery::create(privateTypes, this)){
        auto devices = deviceDiscovery->scanConnectedDevices();
        for(const QString& device : qAsConst(devices)){
            deviceDetected(publicType, device);
        }
        connect(
            deviceDiscovery,
            &QDeviceDiscovery::deviceDetected,
            this,
            [this, publicType](const QString& device){ deviceDetected(publicType, device); }
        );
        connect(
            deviceDiscovery,
            &QDeviceDiscovery::deviceRemoved,
            this,
            [this, publicType](const QString& device){ deviceRemoved(publicType, device); }
        );
    }
}

void OxideEventManager::deviceDetected(QInputDeviceManager::DeviceType type, const QString& device){
    m_devices[type].append(device);
    auto manager = QGuiApplicationPrivate::inputDeviceManager();
    QInputDeviceManagerPrivate::get(manager)->setDeviceCount(type, m_devices[type].count());
    auto number = QFileInfo(device).baseName().midRef(5).toInt();
    m_handler.add(number, type);
}

void OxideEventManager::deviceRemoved(QInputDeviceManager::DeviceType type, const QString& device){
    m_devices[type].removeAll(device);
    auto manager = QGuiApplicationPrivate::inputDeviceManager();
    QInputDeviceManagerPrivate::get(manager)->setDeviceCount(type, m_devices[type].count());
    auto number = QFileInfo(device).baseName().midRef(5).toInt();
    m_handler.remove(number, type);
}


