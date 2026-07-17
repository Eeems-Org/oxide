#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

QSet<QString> settings = {"columns", "autoStartApplication"};
QSet<QString> booleanSettings{
  "showWifiDb",
  "showBatteryPercent",
  "showBatteryTemperature",
  "showDate"
};
QList<QString> configDirectoryPaths =
  {"/opt/etc/draft", "/etc/draft", "/home/root/.config/draft"};
QList<QString> configFileDirectoryPaths =
  {"/opt/etc", "/etc", "/home/root/.config", "/home/root/.vellum/etc"};

QFile*
getConfigFile() {
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
bool
configFileExists() {
  auto configFile = getConfigFile();
  bool exists = configFile != nullptr && configFile->exists();
  configFile->close();
  delete configFile;
  return exists;
}
void
Controller::loadSettings() {
  // If the config directory doesn't exist,
  // then print an error and stop.
  qDebug() << "parsing config file...";
  if (configFileExists()) {
    auto configFile = getConfigFile();
    if (!configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
      qCritical() << "Couldn't read the config file. "
                  << configFile->fileName();
      return;
    }
    QTextStream in(configFile);
    while (!in.atEnd()) {
      QString line = in.readLine();
      if (!line.startsWith("#") && !line.isEmpty()) {
        QStringList parts = line.split("=");
        if (parts.length() != 2) {
          O_WARNING("  Wrong format on " << line);
          continue;
        }
        QString lhs = parts.at(0).trimmed();
        QString rhs = parts.at(1).trimmed();
        if (booleanSettings.contains(lhs)) {
          auto property = lhs.toStdString();
          auto value = rhs.toLower();
          auto boolValue = value == "true" || value == "t" || value == "y" ||
                           value == "yes" || value == "1";
          setProperty(property.c_str(), boolValue);
          qDebug() << "  " << (property + ":").c_str() << boolValue;
        } else if (settings.contains(lhs)) {
          auto property = lhs.toStdString();
          setProperty(property.c_str(), rhs);
          qDebug() << "  " << (property + ":").c_str()
                   << rhs.toStdString().c_str();
        }
      }
    }
    configFile->close();
    delete configFile;
  }
}
inline void
updateUI(QObject* ui, const char* name, const QVariant& value) {
  if (ui) {
    ui->setProperty(name, value);
  }
}
void
Controller::updateUIElements() {}
void
Controller::setShowWifiDb(bool state) {
  m_showWifiDb = state;
  if (root != nullptr) {
    qDebug() << "Show Wifi DB: " << state;
    emit showWifiDbChanged(state);
  }
  if (state) {
    updateUIElements();
  }
}
void
Controller::setShowBatteryPercent(bool state) {
  m_showBatteryPercent = state;
  if (root != nullptr) {
    qDebug() << "Show Battery Percentage: " << state;
    emit showBatteryPercentChanged(state);
  }
}
void
Controller::setShowBatteryTemperature(bool state) {
  m_showBatteryTemperature = state;
  if (root != nullptr) {
    qDebug() << "Show Battery Temperature: " << state;
    emit showBatteryTemperatureChanged(state);
  }
}

#include "moc_controller.cpp"
