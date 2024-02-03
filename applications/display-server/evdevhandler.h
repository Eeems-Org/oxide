#pragma once

#include <QThread>
#include <unordered_map>
#include <liboxide/event_device.h>

#include "evdevdevice.h"

using namespace Oxide;

#define evdevHandler EvDevHandler::init()

class EvDevHandler : public QThread{
    Q_OBJECT

public:
    static EvDevHandler* init();
    EvDevHandler();
    ~EvDevHandler();

private:
    QList<EvDevDevice*> devices;
    bool hasDevice(event_device device);
    void reloadDevices();
};
