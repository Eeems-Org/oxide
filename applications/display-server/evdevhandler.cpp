#include "evdevhandler.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <liboxide/devicesettings.h>
#include <liboxide/debug.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <cstring>

EvDevHandler* EvDevHandler::init(){
    static EvDevHandler* instance;
    if(instance != nullptr){
        return instance;
    }
    instance = new EvDevHandler();
    instance->moveToThread(instance);
    instance->start();
    return instance;
}

EvDevHandler::EvDevHandler()
: QThread(),
  m_fd(-1)
{
    setObjectName("EvDevHandler");
    reloadDevices();
    deviceSettings.onKeyboardAttachedChanged([this]{ reloadDevices(); });
}

EvDevHandler::~EvDevHandler(){}

void EvDevHandler::writeEvent(int type, int code, int val){
    input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    // timestamp values below are ignored
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    writeEvent(&ie);
}

void EvDevHandler::writeEvent(input_event* ie){
    O_DEBUG("writeEvent(" << ie->type << ie->code << ie->value << ")");
    if(m_fd < 0){
        return;
    }
    int res = -1;
    while(res < 0){
        res = ::send(m_fd, ie, sizeof(ie), MSG_EOR);
        if(res > -1){
            break;
        }
        if(errno == EAGAIN || errno == EINTR){
            timespec remaining;
            timespec requested{
                .tv_sec = 0,
                .tv_nsec = 5000
            };
            nanosleep(&requested, &remaining);
            continue;
        }
        break;
    }
    if(res < 0){
        O_WARNING("Failed to write input event: " << std::strerror(errno));
    }else if(res != sizeof(ie)){
        O_WARNING("Failed to write input event: Size mismatch!");
    }
}

void EvDevHandler::setInputFd(int fd){ m_fd = fd; }


bool EvDevHandler::hasDevice(event_device device){
    for(auto input : qAsConst(devices)){
        if(device.device.c_str() == input->path()){
            return true;
        }
    }
    return false;
}

void EvDevHandler::reloadDevices(){
    O_DEBUG("Reloading keyboards");
    for(auto& device : deviceSettings.inputDevices()){
        if(device.device == deviceSettings.getButtonsDevicePath()){
            continue;
        }
        if(!hasDevice(device) && device.fd != -1){
            auto input = new EvDevDevice(this, device);
            O_DEBUG(input->name() << "added");
            devices.append(input);
            input->readEvents();
        }
    }
    QMutableListIterator<EvDevDevice*> i(devices);
    while(i.hasNext()){
        EvDevDevice* device = i.next();
        if(device->exists()){
            continue;
        }
        O_DEBUG(device->name() << "removed");
        i.remove();
        delete device;
    }
}
