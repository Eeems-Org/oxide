#pragma once

#include <QObject>
#include "appitem.h"
#include "eventfilter.h"

class Controller : public QObject
{
    Q_OBJECT

public:
    EventFilter* filter;
    explicit Controller(QObject* parent = 0) : QObject(parent){}
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void killXochitl();
    Q_INVOKABLE QString getBatteryLevel();
    Q_INVOKABLE void resetInactiveTimer();
};
