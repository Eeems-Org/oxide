#pragma once

#include <QtInputSupport/private/devicehandlerlist_p.h>
#include <private/qinputdevicemanager_p_p.h>

#include <QObject>

#include "oxideeventhandler.h"
#include "private/qdevicediscovery_p.h"

#define THREADED_INPUT
#ifdef THREADED_INPUT
#include <QThread>
#endif

class OxideEventManager : public QObject {
  Q_OBJECT
public:
  OxideEventManager(const QStringList& parameters);
  ~OxideEventManager();

private slots:
  void
  deviceDetected(QInputDeviceManager::DeviceType type, const QString& device);
  void
  deviceRemoved(QInputDeviceManager::DeviceType type, const QString& device);
  void deviceListChanged(QInputDeviceManager::DeviceType type);

private:
  void setup(
    QDeviceDiscovery::QDeviceTypes privateTypes,
    QInputDeviceManager::DeviceType publicType
  );
  QMap<QInputDeviceManager::DeviceType, QStringList> m_devices;
  OxideEventHandler* m_handler = nullptr;
#ifdef THREADED_INPUT
  QThread* m_thread = nullptr;
#endif
};
