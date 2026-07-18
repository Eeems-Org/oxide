#pragma once

#include <QObject>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>

#include <liboxide.h>
#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

class DisplayController : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    bool showBatteryPercent MEMBER m_showBatteryPercent WRITE
      setShowBatteryPercent NOTIFY showBatteryPercentChanged
  )
  Q_PROPERTY(
    bool showBatteryTemperature MEMBER m_showBatteryTemperature WRITE
      setShowBatteryTemperature NOTIFY showBatteryTemperatureChanged
  )
  Q_PROPERTY(
    bool showWifiDb MEMBER m_showWifiDb WRITE setShowWifiDb NOTIFY
      showWifiDbChanged
  )
  Q_PROPERTY(
    bool showDate MEMBER m_showDate WRITE setShowDate NOTIFY showDateChanged
  )
  Q_PROPERTY(int columns MEMBER m_columns WRITE setColumns NOTIFY columnsChanged)
  Q_PROPERTY(bool hasFrontlight READ hasFrontlight CONSTANT)
  Q_PROPERTY(
    bool extraBrightness READ extraBrightness WRITE setExtraBrightness NOTIFY
      extraBrightnessChanged
  )
  Q_PROPERTY(int maxTouchWidth READ maxTouchWidth CONSTANT)
  Q_PROPERTY(int maxTouchHeight READ maxTouchHeight CONSTANT)

public:
  explicit DisplayController(
    General* api,
    QObject* parent = nullptr
  )
    : QObject(parent)
    , api(api) {
    loadConfigFile();
  }

  void ensureApi() {
    if (apiReady || api == nullptr) return;
    auto reply = api->requestAPI("frontlight");
    reply.waitForFinished();
    if (reply.isError()) return;
    auto path = ((QDBusObjectPath)reply).path();
    if (path == "/") return;
    frontlightApi = new Frontlight(OXIDE_SERVICE, path, QDBusConnection::systemBus(), this);
    connect(
      frontlightApi,
      &Frontlight::extraBrightnessChanged,
      this,
      &DisplayController::extraBrightnessChanged
    );
    apiReady = true;
  }

  // === Display ===
  bool showBatteryPercent() const { return m_showBatteryPercent; }
  void setShowBatteryPercent(bool v) {
    if (m_showBatteryPercent == v) return;
    m_showBatteryPercent = v;
    emit showBatteryPercentChanged(v);
    saveConfigFile();
  }
  bool showBatteryTemperature() const { return m_showBatteryTemperature; }
  void setShowBatteryTemperature(bool v) {
    if (m_showBatteryTemperature == v) return;
    m_showBatteryTemperature = v;
    emit showBatteryTemperatureChanged(v);
    saveConfigFile();
  }
  bool showWifiDb() const { return m_showWifiDb; }
  void setShowWifiDb(bool v) {
    if (m_showWifiDb == v) return;
    m_showWifiDb = v;
    emit showWifiDbChanged(v);
    saveConfigFile();
  }
  bool showDate() const { return m_showDate; }
  void setShowDate(bool v) {
    if (m_showDate == v) return;
    m_showDate = v;
    emit showDateChanged(v);
    saveConfigFile();
  }
  int columns() const { return m_columns; }
  void setColumns(int v) {
    if (m_columns == v) return;
    m_columns = v;
    emit columnsChanged(v);
    saveConfigFile();
  }
  bool hasFrontlight() {
    ensureApi();
    return frontlightApi != nullptr && frontlightApi->hasFrontlight();
  }
  bool extraBrightness() {
    ensureApi();
    return frontlightApi != nullptr && frontlightApi->extraBrightness();
  }
  void setExtraBrightness(bool enabled) {
    ensureApi();
    if (frontlightApi != nullptr) {
      frontlightApi->setExtraBrightness(enabled);
    }
  }
  int maxTouchWidth() { return deviceSettings.getTouchWidth() * 0.9; }
  int maxTouchHeight() { return deviceSettings.getTouchHeight() * 0.9; }

signals:
  void showBatteryPercentChanged(bool);
  void showBatteryTemperatureChanged(bool);
  void showWifiDbChanged(bool);
  void showDateChanged(bool);
  void columnsChanged(int);
  void extraBrightnessChanged(bool);

private:
  General* api = nullptr;
  Frontlight* frontlightApi = nullptr;
  bool apiReady = false;

  // === Display settings (oxide.conf) ===
  bool m_showBatteryPercent = false;
  bool m_showBatteryTemperature = false;
  bool m_showWifiDb = false;
  bool m_showDate = false;
  int m_columns = 6;

  // === oxide.conf file handling ===
  QList<QString> configFileDirectoryPaths = {
    "/opt/etc", "/etc", "/home/root/.config", "/home/root/.vellum/etc"
  };

  QFile* getConfigFile() {
    for (auto path : configFileDirectoryPaths) {
      QDir dir(path);
      if (dir.exists() && !dir.isEmpty()) {
        QFile* file = new QFile(path + "/oxide.conf");
        if (file->exists()) {
          return file;
        }
      }
    }
    return nullptr;
  }

  void loadConfigFile() {
    auto configFile = getConfigFile();
    if (configFile == nullptr) return;
    if (!configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
      delete configFile;
      return;
    }
    QSet<QString> booleanSettings = {
      "showWifiDb", "showBatteryPercent", "showBatteryTemperature", "showDate"
    };
    QSet<QString> intSettings = {"columns"};
    QTextStream in(configFile);
    while (!in.atEnd()) {
      QString line = in.readLine();
      if (line.startsWith("#") || line.isEmpty()) continue;
      QStringList parts = line.split("=");
      if (parts.length() != 2) continue;
      QString lhs = parts.at(0).trimmed();
      QString rhs = parts.at(1).trimmed();
      if (booleanSettings.contains(lhs)) {
        bool value = rhs.toLower() == "true" || rhs.toLower() == "t" ||
                     rhs.toLower() == "y" || rhs.toLower() == "yes" ||
                     rhs == "1";
        setProperty(lhs.toStdString().c_str(), value);
      } else if (intSettings.contains(lhs)) {
        setProperty(lhs.toStdString().c_str(), rhs.toInt());
      }
    }
    configFile->close();
    delete configFile;
  }

  void saveConfigFile() {
    QFile* configFile = getConfigFile();
    if (configFile == nullptr) {
      configFile =
        new QFile(configFileDirectoryPaths.last() + "/oxide.conf");
    }
    QMap<QString, QString> entries;
    if (configFile->exists()) {
      if (configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(configFile);
        while (!in.atEnd()) {
          QString line = in.readLine();
          if (line.startsWith("#") || line.isEmpty()) {
            entries[line] = "";
            continue;
          }
          QStringList parts = line.split("=");
          if (parts.length() == 2) {
            entries[parts.at(0).trimmed()] = parts.at(1).trimmed();
          }
        }
        configFile->close();
      }
    }
    entries["showBatteryPercent"] = m_showBatteryPercent ? "yes" : "no";
    entries["showBatteryTemperature"] = m_showBatteryTemperature ? "yes" : "no";
    entries["showWifiDb"] = m_showWifiDb ? "yes" : "no";
    entries["showDate"] = m_showDate ? "yes" : "no";
    entries["columns"] = QString::number(m_columns);
    if (!configFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
      delete configFile;
      return;
    }
    QTextStream out(configFile);
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
      if (it.value().isEmpty()) {
        out << it.key() << "\n";
      } else {
        out << it.key() << "=" << it.value() << "\n";
      }
    }
    configFile->close();
    delete configFile;
  }
};
