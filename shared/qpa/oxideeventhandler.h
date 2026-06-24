#pragma once

#include <libblight/types.h>
#include <libblight_protocol/ringbuffer.h>
#include <liboxide/event_device.h>
#include <private/qevdevkeyboardhandler_p.h>
#include <private/qguiapplication_p.h>
#include <private/qinputdevicemanager_p.h>

#include <QObject>
#include <QString>

#include <atomic>
#include <linux/prctl.h>
#include <map>
#include <memory>
#include <mutex>
#include <sys/prctl.h>
#include <thread>
#include <vector>

class OxideEventManager;

class DeviceData : public std::enable_shared_from_this<DeviceData> {
public:
  DeviceData(
    unsigned int device,
    QInputDeviceManager::DeviceType type,
    QObject* handler
  );
  ~DeviceData();
  unsigned int device;
  QInputDeviceManager::DeviceType type;
  std::shared_ptr<Blight::input_buffer_t> buffer;
  std::atomic<bool> stopFlag;
  std::unique_ptr<std::thread> thread;
  template<typename T>
  T* get() {
    return reinterpret_cast<T*>(m_data);
  }

private:
  template<typename T>
  T* set() {
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
  void
  handleInputEvent(std::shared_ptr<DeviceData> data, struct input_event event);

private:
  void processKeyboardEvent(
    const std::shared_ptr<DeviceData>& data,
    struct input_event* event
  );
  void processTabletEvent(
    const std::shared_ptr<DeviceData>& data,
    struct input_event* event
  );
  void processTouchEvent(
    const std::shared_ptr<DeviceData>& data,
    struct input_event* event
  );
  void processPointerEvent(
    const std::shared_ptr<DeviceData>& data,
    struct input_event* event
  );

  OxideEventManager* m_manager;
  QMap<unsigned int, std::shared_ptr<DeviceData>> m_devices;
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
