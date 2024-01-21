#pragma once

#include <liboxide/event_device.h>
#include <liboxide/sysobject.h>

using namespace Oxide;

class EvDevDevice : public QObject{
    Q_OBJECT

public:
    EvDevDevice(QThread* handler, event_device device);
    ~EvDevDevice();
    QString devName();
    QString name();
    QString path();
    QString id();
    bool exists();
    void flood();

public slots:
    void readEvents();

private:
    event_device device;
    SysObject sys;
    QString _name;
    QSocketNotifier* notifier;
    QMap<int, bool> pressed;
};
