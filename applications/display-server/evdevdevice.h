#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <liboxide/event_device.h>
#include <liboxide/sysobject.h>
#include <libevdev/libevdev.h>

using namespace Oxide;

class EvDevDevice : public QObject{
    Q_OBJECT

public:
    EvDevDevice(QThread* handler, const event_device& device);
    ~EvDevDevice();
    QString devName();
    QString name();
    QString path();
    QString id();
    unsigned int number();
    bool exists();
    void lock();
    void unlock();

signals:
    void inputEvents(const std::vector<input_event> events);

public slots:
    void readEvents();

private:
    event_device device;
    SysObject sys;
    QString _name;
    QSocketNotifier* notifier;
    std::vector<input_event> events;
    libevdev* dev;

    void emitSomeEvents();
};
