#ifndef WIFIAPI_H
#define WIFIAPI_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QException>

#include "dbussettings.h"
#include "sysobject.h"

class WifiAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("D-Bus Interface", OXIDE_WIFI_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
public:
    WifiAPI(QObject* parent) : QObject(parent), wlans() {
        QDir dir("/sys/class/net");
        qDebug() << "Looking for wireless devices...";
        for(auto path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
            qDebug() << ("  Checking " + path + "...").toStdString().c_str();
            SysObject item(dir.path() + "/" + path);
            if(item.hasDirectory("wireless")){
                qDebug() << "    Not a wireless devices";
                break;
            }
            wlans.append(item);
        }
    }
    enum State { Unknown, Off, Disconnected, Offline, Online};
    Q_ENUM(State)

    int state(){ return m_state; }
    void setState(int state){
        if(state < Unknown || state > Online){
            throw QException{};
        }
        m_state = state;
        emit stateChanged(state);
    }

Q_SIGNALS:
    void stateChanged(int);

private:
    QList<SysObject> wlans;
    int m_state = Unknown;
};

#endif // WIFIAPI_H
