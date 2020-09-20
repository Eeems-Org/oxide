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
    Q_PROPERTY(ushort signal READ signal NOTIFY signalChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
public:
    WifiNetwork(Network* network, Wifi* api, QObject* parent)
    : QObject(parent),
      api(api),
      network(network),
      m_ssid(network->ssid()),
      m_protocol(network->protocol()),
      m_connected(false){}
    ~WifiNetwork(){
        delete network;
    }
    QString ssid() { return m_ssid; }
    QString protocol() { return m_protocol; }
    QString path() { return network->path(); }
    Q_INVOKABLE void connect(){
        network->connect().waitForFinished();
    }
    Q_INVOKABLE void disconnect(){
        if(api != nullptr){
            api->disconnect().waitForFinished();
            setConnected(false);
            api->scan(true).waitForFinished();
        }
    }
    bool connected() { return m_connected; }
    void setConnected(bool connected){
        if(m_connected != connected){
            m_connected = connected;
            emit connectedChanged(connected);
            emit availableChanged(available());
        }
    }
    ushort signal(){
        auto bus = QDBusConnection::systemBus();
        int result = 0;
        for(auto path : network->bSSs()){
            BSS bss(OXIDE_SERVICE, path.path(), bus);
            auto signal = bss.signal();
            if(signal > result){
                result = signal;
            }
        }
        if(result != m_signal){
            m_signal = result;
            emit signalChanged(result);
        }
        return result;
    }
    bool available(){
        return m_connected || network->bSSs().length() > 0;
    }
    void setAPI(Wifi* api){
        this->api = api;
    }

signals:
    void connectedChanged(bool);
    void fakeStringSignal(QString);
    void signalChanged(ushort);
    void availableChanged(bool);

private:
    Wifi* api;
    Network* network;
    QString m_ssid;
    QString m_protocol;
    bool m_connected;
    ushort m_signal;
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
        beginInsertRows(QModelIndex(), networks.length() - 1, networks.length());
        networks.append(new WifiNetwork(network, api, this));
        endInsertRows();
        sortByLevel();
    }
    void clear(){
        beginRemoveRows(QModelIndex(), 0, networks.length());
        for(auto network : networks){
            delete network;
        }
        networks.clear();
        endRemoveRows();
    }
    void sortByLevel(){
        emit layoutAboutToBeChanged();
        std::sort(networks.begin(), networks.end(), [=](WifiNetwork* a, WifiNetwork* b) -> bool {
            return a->connected() || (a->available() && ! b->available()) || (a->signal() > b->signal());
        });
        for(auto network : networks){
            emit network->availableChanged(network->available());
        }
        emit layoutChanged();
    }
    void remove(const QDBusObjectPath& path){
        QMutableListIterator<WifiNetwork*> i(networks);
        while(i.hasNext()){
            auto network = i.next();
            if(network->path() == path.path()){
                qDebug() << "Network removed" << network->ssid();
                i.remove();
                delete network;
            }
        }
        sortByLevel();
    }
    void setConnected(const QDBusObjectPath& path){
        for(auto network : networks){
            network->setConnected(network->path() == path.path());
        }
        sortByLevel();
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

signals:
    void scanningChanged(bool);

private:
    Wifi* api = nullptr;
    QList<WifiNetwork*> networks;
};

#endif // WIFINETWORK_H
