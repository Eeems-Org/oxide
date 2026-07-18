#pragma once

#include <QObject>
#include <QTimer>

#include <liboxide/dbus.h>

#include "notificationlist.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

class NotificationsController : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    uint notificationDisplayTime MEMBER m_notificationDisplayTime WRITE
      setNotificationDisplayTime NOTIFY notificationDisplayTimeChanged
  )
  Q_PROPERTY(
    NotificationList* notificationsList READ notificationsList CONSTANT
  )

public:
  explicit NotificationsController(General* api, QObject* parent = nullptr)
    : QObject(parent)
    , api(api) {
    m_notificationsList = new NotificationList(this);
  }

  void ensureApi() {
    if (apiReady || api == nullptr) {
      return;
    }
    auto reply = api->requestAPI("notification");
    reply.waitForFinished();
    if (reply.isError()) {
      return;
    }
    auto path = ((QDBusObjectPath)reply).path();
    if (path == "/") {
      return;
    }
    notificationsApi = new Notifications(
      OXIDE_SERVICE, path, QDBusConnection::systemBus(), this
    );
    m_notificationDisplayTime = notificationsApi->displayTime();
    connect(
      notificationsApi,
      &Notifications::displayTimeChanged,
      this,
      [this](uint v) { setNotificationDisplayTime(v); }
    );
    connect(
      notificationsApi,
      &Notifications::notificationAdded,
      this,
      &NotificationsController::notificationAdded
    );
    connect(
      notificationsApi,
      &Notifications::notificationRemoved,
      this,
      &NotificationsController::notificationRemoved
    );
    connect(
      notificationsApi,
      &Notifications::notificationChanged,
      this,
      &NotificationsController::notificationChanged
    );
    // Populate existing notifications
    auto allNotifications = notificationsApi->allNotifications();
    for (const auto& notifPath : allNotifications) {
      auto notification = new Notification(
        OXIDE_SERVICE, notifPath.path(), QDBusConnection::systemBus()
      );
      m_notificationsList->append(notification);
    }
    apiReady = true;
    emit notificationDisplayTimeChanged(m_notificationDisplayTime);
  }

  Q_INVOKABLE void init() { ensureApi(); }

  NotificationList* notificationsList() { return m_notificationsList; }

  uint notificationDisplayTime() { return m_notificationDisplayTime; }
  void setNotificationDisplayTime(uint v) {
    if (m_notificationDisplayTime == v) {
      return;
    }
    m_notificationDisplayTime = v;
    emit notificationDisplayTimeChanged(v);
    if (notificationsApi != nullptr) {
      notificationsApi->setDisplayTime(v);
    }
  }

signals:
  void notificationDisplayTimeChanged(uint);

private slots:
  void notificationAdded(const QDBusObjectPath& path) {
    if (m_notificationsList->get(path) != nullptr) {
      return;
    }
    auto notification = new Notification(
      OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
    );
    m_notificationsList->append(notification);
  }
  void notificationRemoved(const QDBusObjectPath& path) {
    auto notification = m_notificationsList->get(path);
    if (notification == nullptr) {
      return;
    }
    m_notificationsList->removeAll(notification);
  }
  void notificationChanged(const QDBusObjectPath& path) {
    Q_UNUSED(path);
    // Data changes are handled by NotificationItem::changed slot
  }

private:
  General* api = nullptr;
  Notifications* notificationsApi = nullptr;
  NotificationList* m_notificationsList = nullptr;
  bool apiReady = false;
  uint m_notificationDisplayTime = 5;
};
