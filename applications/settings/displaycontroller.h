#pragma once

#include <QObject>

#include <liboxide.h>
#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

class DisplayController : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    bool showBatteryPercent READ showBatteryPercent WRITE setShowBatteryPercent
      NOTIFY showBatteryPercentChanged
  )
  Q_PROPERTY(
    bool showBatteryTemperature READ showBatteryTemperature WRITE
      setShowBatteryTemperature NOTIFY showBatteryTemperatureChanged
  )
  Q_PROPERTY(
    bool showWifiDb READ showWifiDb WRITE setShowWifiDb NOTIFY showWifiDbChanged
  )
  Q_PROPERTY(
    bool showDate READ showDate WRITE setShowDate NOTIFY showDateChanged
  )
  Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
  Q_PROPERTY(bool hasFrontlight READ hasFrontlight CONSTANT)
  Q_PROPERTY(
    int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged
  )
  Q_PROPERTY(
    bool extraBrightness READ extraBrightness WRITE setExtraBrightness NOTIFY
      extraBrightnessChanged
  )
  Q_PROPERTY(int maxTouchWidth READ maxTouchWidth CONSTANT)
  Q_PROPERTY(int maxTouchHeight READ maxTouchHeight CONSTANT)
  Q_PROPERTY(
    uint notificationDisplayTime READ notificationDisplayTime WRITE
      setNotificationDisplayTime NOTIFY notificationDisplayTimeChanged
  )

public:
  explicit DisplayController(
    QObject* parent,
    Notifications* notificationApi,
    Frontlight* frontlightApi
  )
    : QObject(parent)
    , notificationApi(notificationApi)
    , frontlightApi(frontlightApi) {
    deactivate();
    connect(
      frontlightApi,
      &Frontlight::brightnessChanged,
      this,
      &DisplayController::brightnessChanged
    );
    connect(
      frontlightApi,
      &Frontlight::extraBrightnessChanged,
      this,
      &DisplayController::extraBrightnessChanged
    );
    connect(
      notificationApi,
      &Notifications::displayTimeChanged,
      this,
      &DisplayController::notificationDisplayTimeChanged
    );
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::showBatteryPercentChanged,
      this,
      &DisplayController::showBatteryPercentChanged
    );
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::showBatteryTemperatureChanged,
      this,
      &DisplayController::showBatteryTemperatureChanged
    );
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::showWifiDbChanged,
      this,
      &DisplayController::showWifiDbChanged
    );
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::showDateChanged,
      this,
      &DisplayController::showDateChanged
    );
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::columnsChanged,
      this,
      &DisplayController::columnsChanged
    );
  }

  Q_INVOKABLE void activate() {
    frontlightApi->blockSignals(false);
    notificationApi->blockSignals(false);
    emit showBatteryPercentChanged(showBatteryPercent());
    emit showBatteryTemperatureChanged(showBatteryTemperature());
    emit showWifiDbChanged(showWifiDb());
    emit showDateChanged(showDate());
    emit columnsChanged(columns());
    emit brightnessChanged(brightness());
    emit extraBrightnessChanged(extraBrightness());
    emit notificationDisplayTimeChanged(notificationDisplayTime());
  }

  Q_INVOKABLE void deactivate() {
    frontlightApi->blockSignals(true);
    notificationApi->blockSignals(true);
  }

  bool showBatteryPercent() const {
    return sharedSettings.showBatteryPercent();
  }
  void setShowBatteryPercent(bool showBatteryPercent) {
    sharedSettings.set_showBatteryPercent(showBatteryPercent);
  }
  bool showBatteryTemperature() const {
    return sharedSettings.showBatteryTemperature();
  }
  void setShowBatteryTemperature(bool showBatteryTemperature) {
    sharedSettings.set_showBatteryTemperature(showBatteryTemperature);
  }
  bool showWifiDb() const { return sharedSettings.showWifiDb(); }
  void setShowWifiDb(bool showWifiDb) {
    sharedSettings.set_showWifiDb(showWifiDb);
  }
  bool showDate() const { return sharedSettings.showDate(); }
  void setShowDate(bool showDate) { sharedSettings.set_showDate(showDate); }
  int columns() const { return sharedSettings.columns(); }
  void setColumns(int columns) { sharedSettings.set_columns(columns); }
  bool hasFrontlight() { return frontlightApi->hasFrontlight(); }
  int brightness() { return frontlightApi->brightness(); }
  void setBrightness(int brightness) {
    frontlightApi->setBrightness(brightness);
  }
  bool extraBrightness() { return frontlightApi->extraBrightness(); }
  void setExtraBrightness(bool enabled) {
    frontlightApi->setExtraBrightness(enabled);
  }
  int maxTouchWidth() { return deviceSettings.getTouchWidth() * 0.9; }
  int maxTouchHeight() { return deviceSettings.getTouchHeight() * 0.9; }
  uint notificationDisplayTime() { return notificationApi->displayTime(); }
  void setNotificationDisplayTime(uint displayTime) {
    notificationApi->setDisplayTime(displayTime);
  }

signals:
  void showBatteryPercentChanged(bool);
  void showBatteryTemperatureChanged(bool);
  void showWifiDbChanged(bool);
  void showDateChanged(bool);
  void columnsChanged(int);
  void brightnessChanged(int);
  void extraBrightnessChanged(bool);
  void notificationDisplayTimeChanged(uint);

private:
  Notifications* notificationApi = nullptr;
  Frontlight* frontlightApi = nullptr;
};
