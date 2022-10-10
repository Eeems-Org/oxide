#include "power.h"

#include <QObject>
#include <QDir>
#include <QDebug>

QList<SysObject>* _batteries = nullptr;
QList<SysObject>* _chargers = nullptr;

void _setup(){
    if(_batteries != nullptr && _chargers != nullptr){
        return;
    }
    if(_batteries != nullptr){
        _batteries->clear();
    }else{
        _batteries = new QList<SysObject>();
    }
    if(_chargers != nullptr){
        _chargers->clear();
    }else{
        _chargers = new QList<SysObject>();
    }
    QDir dir("/sys/class/power_supply");
    qDebug() << "Looking for batteries and chargers...";
    for(auto path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
        qDebug() << ("  Checking " + path + "...").toStdString().c_str();
        SysObject item(dir.path() + "/" + path);
        if(!item.hasProperty("type")){
            qDebug() << "    Missing type property";
            continue;
        }
        if(item.hasProperty("present") && !item.intProperty("present")){
            qDebug() << "    Either missing present property, or battery is not present";
            continue;
        }
        auto type = item.strProperty("type");
        if(type == "Battery"){
            qDebug() << "    Found Battery!";
            _batteries->append(item);
        }else if(type == "USB" || type == "USB_CDP"){
            qDebug() << "    Found Charger!";
            _chargers->append(item);
        }else{
            qDebug() << "    Unknown type";
        }
    }
}

QList<SysObject>* batteries(){
    _setup();
    return _batteries;
}
QList<SysObject>* chargers(){
    _setup();
    return _chargers;
}
