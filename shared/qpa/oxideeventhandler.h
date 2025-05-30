#pragma once

#include <QString>
#include <QSocketNotifier>

#include <QtCore/private/qthread_p.h>
#include <private/qguiapplication_p.h>
#include <private/qinputdevicemanager_p.h>
#include <private/qevdevkeyboardhandler_p.h>

#include <liboxide/event_device.h>
#include <libblight/types.h>

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

class OxideEventHandler : public QObject {
public:
    OxideEventHandler(OxideEventManager* manager, const QStringList& parameters);
    ~OxideEventHandler();

    void add(unsigned int number, QInputDeviceManager::DeviceType type);
    void remove(unsigned int number, QInputDeviceManager::DeviceType type);

private slots:
    void readyRead();
    void processKeyboardEvent(DeviceData* data, Blight::partial_input_event_t* event);
    void processTabletEvent(DeviceData* data, Blight::partial_input_event_t* event);
    void processTouchEvent(DeviceData* data, Blight::partial_input_event_t* event);
    void processPointerEvent(DeviceData* data, Blight::partial_input_event_t* event);

private:
    OxideEventManager* m_manager;
    QMap<unsigned int, DeviceData*> m_devices;
    int m_fd;
    QSocketNotifier* m_notifier;
    const QEvdevKeyboardMap::Mapping* m_keymap;
    int m_keymap_size;
    const QEvdevKeyboardMap::Composing* m_keycompose;
    int m_keycompose_size;
    bool m_no_zap;
    bool m_do_compose;
    QTransform m_rotate;

    static const QEvdevKeyboardMap::Mapping s_keymap_default[];
    static const QEvdevKeyboardMap::Composing s_keycompose_default[];
    static Qt::KeyboardModifiers toQtModifiers(quint16 mod);

    void unloadKeymap();
    bool loadKeymap(const QString& file);
    void parseKeyParams(const QStringList& parameters);
    void parseTouchParams(const QStringList& parameters);
};
