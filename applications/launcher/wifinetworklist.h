#ifndef WIFINETWORK_H
#define WIFINETWORK_H

#include <QAbstractListModel>

#include "wifiapi_interface.h"
#include "network_interface.h"
#include "bss_interface.h"

#ifndef OXIDE_SERVICE
#define OXIDE_SERVICE "codes.eeems.oxide1"
#endif

using namespace codes::eeems::oxide1;

class WifiNetwork : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString ssid MEMBER m_ssid NOTIFY fakeStringSignal)
    Q_PROPERTY(QString protocol MEMBER m_protocol NOTIFY fakeStringSignal)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool known READ known NOTIFY knownChanged)

public:
    WifiNetwork(Network* network, Wifi* api, QObject* parent)
    : QObject(parent),
      api(api),
      network(network),
      m_ssid(network->ssid()),
      m_protocol(network->protocol()),
      m_connected(false),
      bsss() {}
    WifiNetwork(QString ssid, Wifi* api, QObject* parent)
    : QObject(parent),
      api(api),
      network(nullptr),
      m_ssid(ssid),
      m_connected(false),
      bsss() {}
    ~WifiNetwork(){
        if(network != nullptr){
            delete network;
        }
        QMutableListIterator<BSS*> i(bsss);
        while(i.hasNext()){
            auto bss = i.next();
            i.remove();
            delete bss;
        }
    }
    QString ssid() { return m_ssid; }
    QString protocol() { return m_protocol; }
    QStringList paths() {
        QStringList result;
        if(network != nullptr){
            result << network->path();
        }
        for(auto bss : bsss){
            result << bss->path();
        }
        return result;
    }
    Q_INVOKABLE void connect(){
        if(network != nullptr){
            network->connect();
        }else if(bsss.length()){
            bsss.first()->connect();
        }
    }
    Q_INVOKABLE void disconnect(){
        if(api != nullptr){
            api->disconnect().waitForFinished();
            setConnected(false);
            api->scan(true);
        }
    }
    Q_INVOKABLE void remove(){
        if(network != nullptr){
            network->remove().waitForFinished();
        }
    }
    bool connected() { return m_connected; }
    void setConnected(bool connected){
        if(m_connected != connected){
            m_connected = connected;
            emit connectedChanged(connected);
        }
        emit availableChanged(available());
    }
    bool available(){
        return m_connected || bsss.length() || (network != nullptr && network->bSSs().length() > 0);
    }
    bool known(){ return network != nullptr; }
    void setAPI(Wifi* api){
        this->api = api;
    }
    void appendBSS(BSS* bss){
        bsss.append(bss);
    }
    void remove(const QDBusObjectPath& path){
        if(network != nullptr && network->path() == path.path()){
            delete network;
            network = nullptr;
            return;
        }
        QMutableListIterator<BSS*> i(bsss);
        while(i.hasNext()){
            auto bss = i.next();
            if(bss->path() == path.path()){
                i.remove();
                delete bss;
            }
        }
    }
    void setNetwork(Network* network){
        if(this->network){
            delete this->network;
        }
        this->network = network;
    }

signals:
    void connectedChanged(bool);
    void fakeStringSignal(QString);
    void availableChanged(bool);
    void knownChanged(bool);

private:
    Wifi* api;
    Network* network;
    QString m_ssid;
    QString m_protocol;
    bool m_connected;
    QList<BSS*> bsss;
};

class WifiNetworkList : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged);

public:
    explicit WifiNetworkList() : QAbstractListModel(nullptr), networks() {}

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        Q_UNUSED(section)
        Q_UNUSED(orientation)
        Q_UNUSED(role)
        return QVariant();
    }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override{
        if(parent.isValid()){
            return 0;
        }
        return networks.length();
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override{
        if(!index.isValid()){
            return QVariant();
        }
        if(index.column() > 0){
            return QVariant();
        }
        if(index.row() >= networks.length()){
            return QVariant();
        }
        if(role != Qt::DisplayRole){
            return QVariant();
        }
        return QVariant::fromValue(networks[index.row()]);
    }
    void append(Network* network){
        auto ssid = network->ssid();
        for(auto item : networks){
            if(item->ssid() == ssid){
                item->setNetwork(network);
                return;
            }
        }
        qDebug() << "Adding known network" << network->ssid();
        beginInsertRows(QModelIndex(), networks.length(), networks.length());
        networks.append(new WifiNetwork(network, api, this));
        endInsertRows();
        sort();
    }
    void append(BSS* bss){
        auto ssid = bss->ssid();
        for(auto network : networks){
            if(network->ssid() == ssid){
                return;
            }
        }
        qDebug() << "Adding unknown network" << ssid;
        beginInsertRows(QModelIndex(), networks.length(), networks.length());
        auto wifiNetwork = new WifiNetwork(bss->ssid(), api, this);
        wifiNetwork->appendBSS(bss);
        networks.append(wifiNetwork);
        endInsertRows();
        sort();
    }
    void append(QList<Network*> networks){
        for(auto network : networks){
            auto ssid = network->ssid();
            for(auto item : this->networks){
                if(item->ssid() == ssid){
                    item->setNetwork(network);
                    networks.removeAll(network);
                    break;
                }
            }
        }
        beginInsertRows(QModelIndex(), this->networks.length(), this->networks.length() + networks.length() - 1);
        for(auto network : networks){
            qDebug() << "Adding known network" << network->ssid();
            this->networks.append(new WifiNetwork(network, api, this));
        }
        endInsertRows();
        sort();
    }
    void append(QList<BSS*> bsss){
        QMutableListIterator<BSS*> i(bsss);
        while(i.hasNext()){
            auto bss = i.next();
            auto ssid = bss->ssid();
            if(ssid.isEmpty()){
                i.remove();
                continue;
            }
            for(auto network : this->networks){
                if(network->ssid() == ssid){
                    i.remove();
                    break;
                }
            }
        }
        beginInsertRows(QModelIndex(), this->networks.length(), this->networks.length() + bsss.length() - 1);
        for(auto bss : bsss){
            qDebug() << "Adding unknown network" << bss->ssid();
            auto wifiNetwork = new WifiNetwork(bss->ssid(), api, this);
            wifiNetwork->appendBSS(bss);
            networks.append(wifiNetwork);
        }
        endInsertRows();
        sort();
    }
    void clear(){
        beginRemoveRows(QModelIndex(), 0, networks.length());
        for(auto network : networks){
            if(network != nullptr){
                delete network;
            }
        }
        networks.clear();
        endRemoveRows();
    }
    void removeUnknown(){
        QList<WifiNetwork*> toRemove;
        for(auto network : networks){
            if(!network->known()){
                toRemove.append(network);
            }
        }
        QMutableListIterator<WifiNetwork*> i(networks);
        while(i.hasNext()){
            auto network = i.next();
            if(network == nullptr || toRemove.contains(network)){
                auto idx = networks.indexOf(network);
                beginRemoveRows(QModelIndex(), idx, idx);
                qDebug() << "Network removed" << network->ssid();
                i.remove();
                if(network != nullptr){
                    delete network;
                }
                endRemoveRows();
            }
        }
    }
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override{
        Q_UNUSED(column)
        Q_UNUSED(order)
        emit layoutAboutToBeChanged();
        std::sort(networks.begin(), networks.end(), [=](WifiNetwork* a, WifiNetwork* b) -> bool {
            if(a->connected()){
                return true;
            }
            if(b->connected()){
                return false;
            }
            if(a->available() && !b->available()){
                return true;
            }
            if(!a->available() && b->available()){
                return false;
            }
            if(a->known() && !b->known()){
                return true;
            }

            if(!a->known() && b->known()){
                return false;
            }
            return a->ssid() < b->ssid();
        });
        emit layoutChanged();
    }
    Q_INVOKABLE void sort(){
        sort(0);
        for(auto network : networks){
            emit network->availableChanged(network->available());
        }
    }
    void remove(const QDBusObjectPath& path){
        QMutableListIterator<WifiNetwork*> i(networks);
        while(i.hasNext()){
            auto network = i.next();
            network->remove(path);
            if(!network->paths().length()){
                qDebug() << "Network removed" << network->ssid();
                i.remove();
                delete network;
            }
        }
        sort();
    }
    void setConnected(const QDBusObjectPath& path){
        for(auto network : networks){
            network->setConnected(path.path() != "/" && network->paths().contains(path.path()));
        }
        sort();
    }
    void setAPI(Wifi* api){
        this->api = api;
        for(auto network : networks){
            network->setAPI(api);
        }
        connect(api, &Wifi::scanningChanged, this, [=](bool scanning){
            emit scanningChanged(scanning);
        });
    }
    bool scanning(){ return api->scanning(); }
    Q_INVOKABLE void scan(bool active){ api->scan(active).waitForFinished(); }
    Q_INVOKABLE void remove(QString ssid){
        QMutableListIterator<WifiNetwork*> i(networks);
        while(i.hasNext()){
            auto network = i.next();
            if(network->ssid() == ssid){
                beginRemoveRows(QModelIndex(), networks.indexOf(network), networks.indexOf(network));
                i.remove();
                delete network;
                endRemoveRows();
            }
        }
    }

signals:
    void scanningChanged(bool);

private:
    Wifi* api = nullptr;
    QList<WifiNetwork*> networks;
};

#endif // WIFINETWORK_H
