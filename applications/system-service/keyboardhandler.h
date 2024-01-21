#pragma once

#include <linux/uinput.h>
#include <unordered_map>

#include "keyboarddevice.h"

using namespace Oxide;

#define keyboardHandler KeyboardHandler::init()

class EvDevHandler : public QThread{
    Q_OBJECT

public:
    static EvDevHandler* init();
    EvDevHandler();
    ~EvDevHandler();
    void flood();
    void writeEvent(int type, int code, int val);

private slots:
    void keyEvent(int code, int value);

private:
    int fd;
    uinput_setup usetup;
    
    QList<EvDevDevice*> devices;
    bool hasDevice(event_device device);
    void reloadDevices();
};
