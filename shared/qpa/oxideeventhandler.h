#pragma once

#include <QString>
#include <QThread>
#include <QSocketNotifier>

#include <QtCore/private/qthread_p.h>
#include <private/qguiapplication_p.h>
#include <private/qinputdevicemanager_p.h>
#include <private/qevdevkeyboardhandler_p.h>

#include <liboxide/event_device.h>

class OxideEventManager;


class DeviceData{
public:
    DeviceData();
    DeviceData(unsigned int device, QInputDeviceManager::DeviceType type);
    ~DeviceData();
    unsigned int device;
    QInputDeviceManager::DeviceType type;
    template<typename T> T* get(){ return reinterpret_cast<T*>(m_data); }

private:
    template<typename T> T* set(){
        m_data = new T;
        memset(m_data, 0, sizeof(T));
        return get<T>();
    }
    void* m_data = nullptr;
};

class OxideEventHandler : public QDaemonThread {
public:
    OxideEventHandler(OxideEventManager* manager, const QStringList& parameters);
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
    const QEvdevKeyboardMap::Mapping* m_keymap;
    int m_keymap_size;
    const QEvdevKeyboardMap::Composing* m_keycompose;
    int m_keycompose_size;
    bool m_no_zap;
    bool m_do_compose;

    static const QEvdevKeyboardMap::Mapping s_keymap_default[];
    static const QEvdevKeyboardMap::Composing s_keycompose_default[];

    void unloadKeymap();
    bool loadKeymap(const QString& file);
};
