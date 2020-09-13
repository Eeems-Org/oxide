#ifndef BSS_H
#define BSS_H

#include <QMutableListIterator>

#include "supplicant.h"
#include "dbussettings.h"

class BSS : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_NETWORK_INTERFACE)
    Q_PROPERTY(QString bssid READ bssid)
    Q_PROPERTY(QString ssid READ ssid)
public:
    BSS(QString path, QString bssid, QString ssid, QObject* parent);
    BSS(QString path, IBSS* bss, QObject* parent) : BSS(path, bss->bSSID(), bss->sSID(), parent) {}

    ~BSS(){
        QDBusConnection::systemBus().unregisterObject(path());
    }
    QString path(){ return m_path; }

    QString bssid(){ return m_bssid; }
    QString ssid(){ return m_ssid; }

    QList<QString> paths(){
        QList<QString> result;
        for(auto bss : bsss){
            result.append(bss->path());
        }
        return result;
    }
    void addBSS(QString path, Interface* interface){
        if(paths().contains(path)){
            return;
        }
        auto bss = new IBSS(WPA_SUPPLICANT_SERVICE, path, QDBusConnection::systemBus(), interface);
        bsss.append(bss);
        connect(bss, &IBSS::PropertiesChanged, this, &BSS::PropertiesChanged, Qt::QueuedConnection);
    }
    void addBSS(IBSS* bss){
        if(paths().contains(bss->path())){
            return;
        }
        bsss.append(bss);
        connect(bss, &IBSS::PropertiesChanged, this, &BSS::PropertiesChanged, Qt::QueuedConnection);
    }
    void removeBSS(QString path){
        QMutableListIterator<IBSS*> i(bsss);
        while(i.hasNext()){
            auto bss = i.next();
            if(bss->path() == path){
                i.remove();
            }
        }
    }

private slots:
    void PropertiesChanged(const QVariantMap& properties){
        Q_UNUSED(properties);
    }

private:
    QString m_path;
    QList<IBSS*> bsss;
    QString m_bssid;
    QString m_ssid;
};

#endif // BSS_H
