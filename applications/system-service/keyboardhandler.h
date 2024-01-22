#pragma once

#include <linux/uinput.h>
#include <unordered_map>

#include "keyboarddevice.h"

using namespace Oxide;

#define keyboardHandler KeyboardHandler::init()

class KeyboardHandler : public QThread{
    Q_OBJECT

public:
    static KeyboardHandler* init();
    KeyboardHandler();
    ~KeyboardHandler();
    void flood();
    void writeEvent(int type, int code, int val);

private slots:
    void keyEvent(int code, int value);

private:
    int fd;
    uinput_setup usetup;
    
    QList<KeyboardDevice*> devices;
    bool hasDevice(event_device device);
    void reloadDevices();
};
