#pragma once

#include <liboxide.h>

#include <QGuiApplication>
#include <QObject>

#include "appitem.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;
using codes::eeems::oxide1::Power;
using namespace Oxide::Sentry;

enum BatteryState {
  BatteryUnknown,
  BatteryCharging,
  BatteryDischarging,
  BatteryNotPresent
};
enum ChargerState {
  ChargerUnknown,
  ChargerConnected,
  ChargerNotConnected,
  ChargerNotPresent
};

class Controller : public QObject {
  Q_OBJECT
  Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
  Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
  Q_PROPERTY(
    QString autoStartApplication READ autoStartApplication WRITE
      setAutoStartApplication NOTIFY autoStartApplicationChanged
  )
  Q_PROPERTY(
    bool powerOffInhibited READ powerOffInhibited NOTIFY
      powerOffInhibitedChanged
  )
  Q_PROPERTY(
    bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged
  )
public:
  QObject* root = nullptr;
  explicit Controller(QObject* parent = 0);
  Q_INVOKABLE void startup() {
    if (autoStartApplication().isEmpty()) {
      qDebug() << "No auto start application";
      return;
    }
    auto app = getApplication(autoStartApplication());
    if (app == nullptr) {
      qDebug() << "Unable to find auto start application";
      return;
    }
    app->execute();
  }
  Q_INVOKABLE void loadSettings() {}
  Q_INVOKABLE void saveSettings() {}
  Q_INVOKABLE QList<QObject*> getApps();
  Q_INVOKABLE void importDraftApps();
  Q_INVOKABLE void suspend();
  Q_INVOKABLE void lock();
  Q_INVOKABLE void
  breadcrumb(QString category, QString message, QString type = "default") {
#ifdef SENTRY
    sentry_breadcrumb(
      category.toStdString().c_str(),
      message.toStdString().c_str(),
      type.toStdString().c_str()
    );
#else
    Q_UNUSED(category);
    Q_UNUSED(message);
    Q_UNUSED(type);
#endif
  }
  int columns() const { return sharedSettings.columns(); }
  void setColumns(int columns) { sharedSettings.set_columns(columns); }
  QString locale() { return deviceSettings.getLocale(); }
  void setLocale(const QString& locale) {
    deviceSettings.setLocale(locale);
    emit localeChanged(locale);
  }
  QString autoStartApplication() {
    return sharedSettings.autoStartApplication();
  }
  void setAutoStartApplication(QString autoStartApplication) {
    sharedSettings.set_autoStartApplication(autoStartApplication);
  }
  bool powerOffInhibited() { return systemApi->powerOffInhibited(); }
  bool sleepInhibited() { return systemApi->sleepInhibited(); }
  Apps* getAppsApi() { return appsApi; }
  Notifications* getNotificationApi() { return notificationApi; }

signals:
  void reload();
  void columnsChanged(int);
  void localeChanged(QString);
  void autoStartApplicationChanged(QString);
  void powerOffInhibitedChanged(bool);
  void sleepInhibitedChanged(bool);

private slots:
  void unregisterApplication(QDBusObjectPath path) {
    auto pathString = path.path();
    for (auto app : applications) {
      if (app->property("path") == pathString) {
        applications.removeAll(app);
        delete app;
        emit reload();
        qDebug() << "Removed" << pathString << "application";
        return;
      }
    }
    qDebug() << "Unable to find application " << pathString << "to remove";
  }
  void registerApplication(QDBusObjectPath path) {
    qDebug() << "New application detected" << path.path();
    emit reload();
  }
  void batteryAlert() {
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("alert", true);
    }
  }
  void batteryLevelChanged(int level) {
    qDebug() << "Battery level: " << level;
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("level", level);
    }
  }
  void batteryStateChanged(int state) {
    switch (state) {
      case BatteryCharging:
        qDebug() << "Battery state: Charging";
        break;
      case BatteryNotPresent:
        qDebug() << "Battery state: Not Present";
        break;
      case BatteryDischarging:
        qDebug() << "Battery state: Discharging";
        break;
      case BatteryUnknown:
      default:
        qDebug() << "Battery state: Unknown";
    }
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      if (state != BatteryNotPresent) {
        ui->setProperty("present", true);
      }
      switch (state) {
        case BatteryCharging:
          ui->setProperty("charging", true);
          break;
        case BatteryNotPresent:
          ui->setProperty("present", false);
          break;
        case BatteryDischarging:
          ui->setProperty("charging", false);
          break;
        case BatteryUnknown:
        default:
          ui->setProperty("charging", false);
      }
    }
  }
  void batteryTemperatureChanged(int temperature) {
    qDebug() << "Battery temperature: " << temperature;
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("temperature", temperature);
    }
  }
  void batteryWarning() {
    qDebug() << "Battery Warning!";
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("warning", true);
    }
  }
  void chargerStateChanged(int state) {
    switch (state) {
      case ChargerConnected:
        qDebug() << "Charger state: Connected";
        break;
      case ChargerNotPresent:
        qDebug() << "Charger state: Not Present";
        break;
      case ChargerNotConnected:
        qDebug() << "Charger state: Not Connected";
        break;
      case ChargerUnknown:
      default:
        qDebug() << "Charger state: Unknown";
    }
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      if (state != ChargerNotPresent) {
        ui->setProperty("present", true);
      }
      switch (state) {
        case ChargerConnected:
          ui->setProperty("connected", true);
          break;
        case ChargerNotConnected:
        case ChargerNotPresent:
          ui->setProperty("connected", false);
          break;
        case ChargerUnknown:
        default:
          ui->setProperty("connected", false);
      }
    }
  }

private:
  General* api = nullptr;
  Power* powerApi = nullptr;
  System* systemApi = nullptr;
  Apps* appsApi = nullptr;
  Notifications* notificationApi = nullptr;
  QList<QObject*> applications;
  AppItem* getApplication(QString name);
};
