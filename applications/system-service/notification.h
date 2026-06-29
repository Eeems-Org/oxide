#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <liboxide.h>

#include <QImage>
#include <QObject>
#include <QQuickWindow>
#include <QtDBus>

#include "application.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

class Notification : public QObject {
  Q_OBJECT
  Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
  Q_CLASSINFO("D-Bus Interface", OXIDE_NOTIFICATION_INTERFACE)
  Q_PROPERTY(QString identifier READ identifier)
  Q_PROPERTY(int created READ created)
  Q_PROPERTY(QString application READ application WRITE setApplication)
  Q_PROPERTY(QString text READ text WRITE setText)
  Q_PROPERTY(QString icon READ icon WRITE setIcon)
  Q_PROPERTY(QVariantMap actions READ actions WRITE setActions)

public:
  Notification(
    const QString& path,
    const QString& identifier,
    const QString& owner,
    const QString& application,
    const QString& text,
    const QString& icon,
    QObject* parent
  );
  ~Notification();
  QString path();
  QDBusObjectPath qPath();
  void registerPath();
  void unregisterPath();

  QString identifier();
  int created();
  QString application();
  void setApplication(QString application);
  QString text();
  void setText(QString text);
  QString icon();
  void setIcon(QString icon);

  QString owner();
  void setOwner(QString owner);

  Q_INVOKABLE void display();
  Q_INVOKABLE void remove();
  Q_INVOKABLE void click(const QString& action = "");
  void paintNotification();

  QVariantMap actions();
  void setActions(const QVariantMap& actions);

signals:
  void changed(QVariantMap);
  void removed();
  void displayed();
  void clicked(const QString& action);

private:
  QString m_path;
  QString m_identifier;
  int m_created;
  QString m_owner;
  QString m_application;
  QString m_text;
  QString m_icon;
  QVariantMap m_actions;

  bool
  hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
  void nextNotification(QQuickWindow* window);
};

#endif // NOTIFICATION_H
