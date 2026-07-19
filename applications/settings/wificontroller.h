#pragma once

#include <QObject>

#include <liboxide.h>
#include <liboxide/dbus.h>

#include "wifinetworklist.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

enum WifiState {
  WifiUnknown,
  WifiOff,
  WifiDisconnected,
  WifiOffline,
  WifiOnline
};

class WifiController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool wifiOn READ wifiOn NOTIFY wifiOnChanged)
  Q_PROPERTY(
    WifiNetworkList* networks MEMBER networks READ getNetworks NOTIFY
      networksChanged
  )

public:
  explicit WifiController(QObject* parent, Wifi* wifiApi)
    : QObject(parent)
    , wifiApi(wifiApi) {
    deactivate();
    connect(
      wifiApi,
      &Wifi::stateChanged,
      this,
      &WifiController::wifiStateChanged,
      Qt::UniqueConnection
    );
    connect(
      wifiApi,
      &Wifi::networkConnected,
      this,
      &WifiController::networkConnected,
      Qt::UniqueConnection
    );
    connect(
      wifiApi,
      &Wifi::disconnected,
      this,
      &WifiController::disconnected,
      Qt::UniqueConnection
    );
    connect(
      wifiApi,
      &Wifi::bssFound,
      this,
      &WifiController::bssFound,
      Qt::UniqueConnection
    );
    connect(
      wifiApi,
      &Wifi::bssRemoved,
      this,
      &WifiController::bssRemoved,
      Qt::UniqueConnection
    );
    connect(
      wifiApi,
      &Wifi::networkAdded,
      this,
      &WifiController::networkAdded,
      Qt::UniqueConnection
    );
    connect(
      wifiApi,
      &Wifi::networkRemoved,
      this,
      &WifiController::networkRemoved,
      Qt::UniqueConnection
    );
  }

  bool wifiOn() {
    auto state = wifiApi->state();
    return state != WifiState::WifiOff && state != WifiState::WifiUnknown;
  }
  Q_INVOKABLE bool turnOnWifi() {
    activate();
    return wifiApi->enable();
  }
  Q_INVOKABLE void turnOffWifi() {
    activate();
    wifiApi->disable();
    networks->removeUnknown();
    wifiApi->flushBSSCache(0);
  }
  WifiNetworkList* getNetworks() { return networks; }
  Q_INVOKABLE void deactivate() {
    if (networks != nullptr) {
      networks->deleteLater();
      networks = nullptr;
    }
    emit networksChanged(nullptr);
    wifiApi->blockSignals(true);
  }

  Q_INVOKABLE void activate() {
    networks = new WifiNetworkList();
    networks->setAPI(wifiApi);
    wifiApi->blockSignals(false);
    networks->blockSignals(false);
    emit wifiOnChanged(wifiOn());
    QList<Network*> networksToAdd;
    for (auto path : wifiApi->networks()) {
      auto network = new Network(
        OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
      );
      auto ssid = network->ssid();
      if (ssid.isEmpty()) {
        delete network;
        continue;
      }
      networksToAdd.append(network);
    }
    networks->append(networksToAdd);
    QList<BSS*> bsssToAdd;
    for (auto path : wifiApi->bSSs()) {
      auto bss =
        new BSS(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
      auto ssid = bss->ssid();
      if (ssid.isEmpty()) {
        delete bss;
        continue;
      }
      bsssToAdd.append(bss);
    }
    networks->append(bsssToAdd);
    networks->sort();
    networks->setConnected(wifiApi->network());
    emit networksChanged(networks);
  }

signals:
  void wifiOnChanged(bool);
  void networksChanged(WifiNetworkList*);

private slots:
  void wifiStateChanged(int state) {
    bool on = state != WifiState::WifiOff && state != WifiState::WifiUnknown;
    emit wifiOnChanged(on);
  }
  void networkConnected(const QDBusObjectPath& path) {
    networks->setConnected(path);
  }
  void disconnected() { networks->setConnected(QDBusObjectPath("/")); }
  void bssFound(const QDBusObjectPath& path) {
    auto bss =
      new BSS(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
    auto ssid = bss->ssid();
    if (ssid.isEmpty()) {
      delete bss;
      return;
    }
    networks->append(bss);
  }
  void bssRemoved(const QDBusObjectPath& path) { networks->remove(path); }
  void networkAdded(const QDBusObjectPath& path) {
    auto network = new Network(
      OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
    );
    auto ssid = network->ssid();
    if (ssid.isEmpty()) {
      delete network;
      return;
    }
    networks->append(network);
  }
  void networkRemoved(const QDBusObjectPath& path) { networks->remove(path); }

private:
  Wifi* wifiApi = nullptr;
  WifiNetworkList* networks = nullptr;
};
