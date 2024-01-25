#pragma once

#include <QString>
#include <QThread>
#include <QSocketNotifier>

#include <private/qguiapplication_p.h>
#include <private/qinputdevicemanager_p.h>
#include <QtCore/private/qthread_p.h>

#include <liboxide/event_device.h>

class OxideEventManager;

class DeviceData{
public:
    DeviceData();
    DeviceData(unsigned int device, QInputDeviceManager::DeviceType type);
    ~DeviceData();
    unsigned int device;
    QInputDeviceManager::DeviceType type;
    void* data = nullptr;
};

class OxideEventHandler : public QDaemonThread {
public:
    OxideEventHandler(OxideEventManager* manager);
    ~OxideEventHandler();

    void add(unsigned int number, QInputDeviceManager::DeviceType type);
    void remove(unsigned int number, QInputDeviceManager::DeviceType type);

private slots:
    void readyRead();
    void processKeyboardEvent(DeviceData* data, input_event* event);
    void processTabletEvent(DeviceData* data, input_event* event);
    void processTouchEvent(DeviceData* data, input_event* event);
    void processPointerEvent(DeviceData* data, input_event* event);

private:
    OxideEventManager* m_manager;
    QMap<unsigned int, DeviceData> m_devices;
    int m_fd;
    QSocketNotifier* m_notifier;
};
