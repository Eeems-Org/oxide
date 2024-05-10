#pragma once

#include <liboxide/event_device.h>
#include <liboxide/sysobject.h>

using namespace Oxide;

class KeyboardDevice : public QObject{
    Q_OBJECT

public:
    KeyboardDevice(QThread* handler, event_device device);
    ~KeyboardDevice();
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
