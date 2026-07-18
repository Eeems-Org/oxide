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
  Q_PROPERTY(bool wifiOn MEMBER m_wifion READ wifiOn NOTIFY wifiOnChanged)
  Q_PROPERTY(
    WifiNetworkList* networks MEMBER networks READ getNetworks NOTIFY
      networksChanged
  )

public:
  explicit WifiController(General* api, QObject* parent = nullptr)
    : QObject(parent)
    , api(api) {}

  bool wifiOn() { return m_wifion; }
  Q_INVOKABLE bool turnOnWifi() {
    connectWifiSignals();
    if (wifiApi == nullptr || !wifiApi->enable()) {
      return false;
    }
    m_wifion = true;
    emit wifiOnChanged(true);
    return true;
  }
  Q_INVOKABLE void turnOffWifi() {
    connectWifiSignals();
    if (wifiApi == nullptr) {
      return;
    }
    wifiApi->disable();
    m_wifion = false;
    emit wifiOnChanged(false);
    networks->removeUnknown();
    wifiApi->flushBSSCache(0);
  }
  WifiNetworkList* getNetworks() { return networks; }
  Q_INVOKABLE void disconnectWifiSignals() {
    if (wifiApi == nullptr) {
      return;
    }
    if (networks != nullptr) {
      networks->deleteLater();
      networks = nullptr;
    }
    emit networksChanged(nullptr);
    emit wifiOnChanged(false);
    wifiApi->blockSignals(true);
  }

  Q_INVOKABLE void connectWifiSignals() {
    if (api == nullptr) {
      return;
    }
    if (wifiApi == nullptr) {
      auto reply = api->requestAPI("wifi");
      reply.waitForFinished();
      if (reply.isError()) {
        return;
      }
      auto path = ((QDBusObjectPath)reply).path();
      if (path == "/") {
        return;
      }
      wifiApi =
        new Wifi(OXIDE_SERVICE, path, QDBusConnection::systemBus(), this);
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
    networks = new WifiNetworkList();
    networks->setAPI(wifiApi);
    wifiApi->blockSignals(false);
    networks->blockSignals(false);
    auto state = wifiApi->state();
    m_wifion = state != WifiState::WifiOff && state != WifiState::WifiUnknown;
    emit wifiOnChanged(m_wifion);
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
    if (m_wifion != on) {
      m_wifion = on;
      emit wifiOnChanged(on);
    }
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
  General* api = nullptr;
  Wifi* wifiApi = nullptr;
  bool m_wifion = false;
  WifiNetworkList* networks = nullptr;
};
