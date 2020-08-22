#pragma once

#include <QObject>
#include "appitem.h"

class Controller : public QObject
{
    Q_OBJECT

public:
    QObject* root;
    explicit Controller(QObject* parent = 0) : QObject(parent){}
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE QString getBatteryLevel();


};
