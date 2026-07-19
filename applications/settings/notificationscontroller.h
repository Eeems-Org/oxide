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
    NotificationList* notificationsList READ notificationsList CONSTANT
  )

public:
  explicit NotificationsController(
    QObject* parent,
    Notifications* notificationApi
  )
    : QObject(parent)
    , notificationApi(notificationApi) {
    m_notificationsList = new NotificationList(this);
    deactivate();
    connect(
      notificationApi,
      &Notifications::notificationAdded,
      this,
      &NotificationsController::notificationAdded,
      Qt::UniqueConnection
    );
    connect(
      notificationApi,
      &Notifications::notificationRemoved,
      this,
      &NotificationsController::notificationRemoved,
      Qt::UniqueConnection
    );
    connect(
      notificationApi,
      &Notifications::notificationChanged,
      this,
      &NotificationsController::notificationChanged,
      Qt::UniqueConnection
    );
  }

  Q_INVOKABLE void activate() {
    if (notificationApi == nullptr) {
      return;
    }
    notificationApi->blockSignals(false);
    auto allNotifications = notificationApi->allNotifications();
    for (const auto& notifPath : allNotifications) {
      auto notification = new Notification(
        OXIDE_SERVICE, notifPath.path(), QDBusConnection::systemBus()
      );
      m_notificationsList->append(notification);
    }
  }

  Q_INVOKABLE void deactivate() {
    if (notificationApi == nullptr) {
      return;
    }
    notificationApi->blockSignals(true);
    m_notificationsList->clear();
  }

  NotificationList* notificationsList() { return m_notificationsList; }

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
  Notifications* notificationApi = nullptr;
  NotificationList* m_notificationsList = nullptr;
};
