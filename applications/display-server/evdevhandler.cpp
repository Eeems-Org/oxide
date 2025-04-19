#include "evdevhandler.h"
#include "dbusinterface.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <liboxide/devicesettings.h>
#include <liboxide/debug.h>
#include <liboxide/threading.h>
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
    Oxide::startThreadWithPriority(instance, QThread::HighestPriority);
    return instance;
}

EvDevHandler::EvDevHandler()
: QThread(),
  m_clearing{false}
{
    setObjectName("EvDevHandler");
    reloadDevices();
    auto deviceDiscovery = QDeviceDiscovery::create(QDeviceDiscovery::Device_InputMask, this);
    connect(deviceDiscovery, &QDeviceDiscovery::deviceDetected, this, &EvDevHandler::reloadDevices);
    connect(deviceDiscovery, &QDeviceDiscovery::deviceRemoved, this, &EvDevHandler::reloadDevices);
}

EvDevHandler::~EvDevHandler(){}

void EvDevHandler::clear_buffers(){
    m_clearing = true;
    for(auto input : qAsConst(devices)){
        input->clear_buffer();
    }
    m_clearing = false;
}

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
        if(!hasDevice(device) && device.fd > 0){
            EvDevDevice* input;
            try{
                input = new EvDevDevice(this, device);
            }catch(const std::bad_alloc&){
                O_WARNING("Failed to create input device handler, not enough memory");
                continue;
            }
            connect(input, &EvDevDevice::inputEvents, this, [this, input](auto events){
                if(!m_clearing){
                    dbusInterface->inputEvents(input->number(), events);
                }
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
