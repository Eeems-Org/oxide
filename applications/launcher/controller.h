#pragma once

#include <liboxide.h>

#include "appitem.h"
#include "applistmodel.h"
#include <QGuiApplication>
#include <QObject>

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
  Q_PROPERTY(AppListModel* appList READ appList NOTIFY appListChanged)
public:
  QObject* root = nullptr;
  explicit Controller(QObject* parent = 0);
  void setRoot(QObject* newRoot);
  Q_INVOKABLE void startup();
  Q_INVOKABLE void loadSettings() {}
  Q_INVOKABLE void saveSettings() {}
  Q_INVOKABLE void refreshApps();
  Q_INVOKABLE void importDraftApps();
  Q_INVOKABLE void suspend();
  Q_INVOKABLE void lock();
  Q_INVOKABLE void
  breadcrumb(QString category, QString message, QString type = "default");
  int columns() const { return sharedSettings.columns(); }
  void setColumns(int columns) { sharedSettings.set_columns(columns); }
  QString locale() { return deviceSettings.getLocale(); }
  void setLocale(const QString& locale);
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
  AppListModel* appList() { return m_appList; }

signals:
  void appListChanged();
  void columnsChanged(int);
  void localeChanged(QString);
  void autoStartApplicationChanged(QString);
  void powerOffInhibitedChanged(bool);
  void sleepInhibitedChanged(bool);

private slots:
  void unregisterApplication(QDBusObjectPath path);
  void registerApplication(QDBusObjectPath path);
  void batteryAlert();
  void batteryLevelChanged(int level);
  void batteryStateChanged(int state);
  void batteryTemperatureChanged(int temperature);
  void batteryWarning();
  void chargerStateChanged(int state);

private:
  void scheduleRefresh();
  General* api = nullptr;
  Power* powerApi = nullptr;
  System* systemApi = nullptr;
  Apps* appsApi = nullptr;
  Notifications* notificationApi = nullptr;
  AppListModel* m_appList = nullptr;
  QTimer* m_refreshTimer = nullptr;
};
