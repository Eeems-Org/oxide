#include "keyboardhandler.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <liboxide/devicesettings.h>
#include <liboxide/debug.h>
#include <linux/input.h>

KeyboardHandler* KeyboardHandler::init(){
    static KeyboardHandler* instance;
    if(instance != nullptr){
        return instance;
    }
    instance = new KeyboardHandler();
    instance->moveToThread(instance);
    instance->start();
    return instance;
}

KeyboardHandler::KeyboardHandler(){
    setObjectName("BlightVirtKeyboard");
    reloadDevices();
    deviceSettings.onKeyboardAttachedChanged([this]{ reloadDevices(); });
}

KeyboardHandler::~KeyboardHandler(){}

void KeyboardHandler::writeEvent(int type, int code, int val){
    input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    // timestamp values below are ignored
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    // TODO call connection->inputEvent(ie);
}

void KeyboardHandler::keyEvent(int code, int value){
    writeEvent(EV_KEY, code, value);
    writeEvent(EV_SYN, SYN_REPORT, 0);
}

bool KeyboardHandler::hasDevice(event_device device){
    for(auto keyboard : qAsConst(devices)){
        if(device.device.c_str() == keyboard->path()){
            return true;
        }
    }
    return false;
}

void KeyboardHandler::reloadDevices(){
    O_DEBUG("Reloading keyboards");
    for(auto& device : deviceSettings.keyboards()){
        if(device.device == deviceSettings.getButtonsDevicePath()){
            continue;
        }
        if(!hasDevice(device) && device.fd != -1){
            auto keyboard = new KeyboardDevice(this, device);
            O_DEBUG(keyboard->name() << "added");
            devices.append(keyboard);
            keyboard->readEvents();
        }
    }
    QMutableListIterator<KeyboardDevice*> i(devices);
    while(i.hasNext()){
        KeyboardDevice* device = i.next();
        if(device->exists()){
            continue;
        }
        O_DEBUG(device->name() << "removed");
        i.remove();
        delete device;
    }
}
