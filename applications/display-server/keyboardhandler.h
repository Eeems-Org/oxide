#pragma once

#include <QThread>
#include <unordered_map>
#include <liboxide/event_device.h>

#include "keyboarddevice.h"

using namespace Oxide;

#define keyboardHandler KeyboardHandler::init()

class KeyboardHandler : public QThread{
    Q_OBJECT

public:
    static KeyboardHandler* init();
    KeyboardHandler();
    ~KeyboardHandler();
    void writeEvent(int type, int code, int val);

private slots:
    void keyEvent(int code, int value);

private:
    QList<KeyboardDevice*> devices;
    bool hasDevice(event_device device);
    void reloadDevices();
};
