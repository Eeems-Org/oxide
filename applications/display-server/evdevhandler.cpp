#include "evdevhandler.h"
#include "dbusinterface.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <liboxide/devicesettings.h>
#include <liboxide/debug.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <cstring>

#include <private/qdevicediscovery_p.h>

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
: QThread()
{
    setObjectName("EvDevHandler");
    reloadDevices();
    auto deviceDiscovery = QDeviceDiscovery::create(QDeviceDiscovery::Device_InputMask, this);
    connect(deviceDiscovery, &QDeviceDiscovery::deviceDetected, this, &EvDevHandler::reloadDevices);
    connect(deviceDiscovery, &QDeviceDiscovery::deviceRemoved, this, &EvDevHandler::reloadDevices);
}

EvDevHandler::~EvDevHandler(){}

bool EvDevHandler::hasDevice(event_device device){
    for(auto input : qAsConst(devices)){
        if(device.device.c_str() == input->path()){
            return true;
        }
    }
    return false;
}

void EvDevHandler::reloadDevices(){
    O_DEBUG("Reloading devices");
    for(auto& device : deviceSettings.inputDevices()){
        if(device.device == deviceSettings.getButtonsDevicePath()){
            continue;
        }
        if(!hasDevice(device) && device.fd > 0){
            auto input = new EvDevDevice(this, device);
            connect(input, &EvDevDevice::inputEvents, this, [this, input](auto events){
                dbusInterface->inputEvents(input->number(), events);
            }, Qt::QueuedConnection);
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
