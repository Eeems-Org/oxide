#pragma once

#include <QObject>
#include <QTimer>

#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

class NotificationsController : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    uint notificationDisplayTime MEMBER m_notificationDisplayTime WRITE
      setNotificationDisplayTime NOTIFY notificationDisplayTimeChanged
  )

public:
  explicit NotificationsController(
    General* api,
    QObject* parent = nullptr
  )
    : QObject(parent)
    , api(api) {}

  void ensureApi() {
    if (apiReady || api == nullptr) return;
    auto reply = api->requestAPI("notification");
    reply.waitForFinished();
    if (reply.isError()) return;
    auto path = ((QDBusObjectPath)reply).path();
    if (path == "/") return;
    notificationsApi = new Notifications(OXIDE_SERVICE, path, QDBusConnection::systemBus(), this);
    m_notificationDisplayTime = notificationsApi->displayTime();
    connect(
      notificationsApi,
      &Notifications::displayTimeChanged,
      this,
      [this](uint v) { setNotificationDisplayTime(v); }
    );
    apiReady = true;
    emit notificationDisplayTimeChanged(m_notificationDisplayTime);
  }

  Q_INVOKABLE void init() { ensureApi(); }

  uint notificationDisplayTime() { return m_notificationDisplayTime; }
  void setNotificationDisplayTime(uint v) {
    if (m_notificationDisplayTime == v) return;
    m_notificationDisplayTime = v;
    emit notificationDisplayTimeChanged(v);
    if (notificationsApi != nullptr) {
      notificationsApi->setDisplayTime(v);
    }
  }

signals:
  void notificationDisplayTimeChanged(uint);

private:
  General* api = nullptr;
  Notifications* notificationsApi = nullptr;
  bool apiReady = false;
  uint m_notificationDisplayTime = 5;
};
