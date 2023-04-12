#include "event_device.h"

namespace Oxide {
    input_event event_device::create_event(ushort type, ushort code, int value){
        struct input_event event;
        event.type = type;
        event.code = code;
        event.value = value;
        return event;
    }

    event_device::event_device(string path, int flags) : device(path), flags(flags){
        this->open();
    }
    event_device::~event_device(){
        this->close();
    }

    void event_device::open(){
        if(fd > 0){
            this->close();
        }
        fd = ::open(device.c_str(), flags);
        error = fd < 0 ? errno : 0;
    }

    void event_device::close(){
        if(fd < 0){
            ::close(fd);
        }
        fd = 0;
    }

    int event_device::lock(){
        qDebug() << "locking " << device.c_str();
        int result = ioctl(fd, EVIOCGRAB, 1);
        if(result == EBUSY){
            qDebug() << "Device is busy";
        }else if(result != 0){
            qDebug() << "Unknown error: " << result;
        }else{
            locked = true;
            qDebug() << device.c_str() << " locked";
        }
        return result;
    }

    int event_device::unlock(){
        int result = ioctl(fd, EVIOCGRAB, 0);
        if(result){
            qDebug() << "Failed to unlock " << device.c_str() << ": " << result;
        }else{
            locked = false;
            qDebug() << "Unlocked " << device.c_str();
        }
        return result;
    }

    void event_device::write(input_event ie){
    #ifdef DEBUG
        qDebug() << "WRITE: " << ie.type << ", " << ie.code << ", " << ie.value << " to " << device.c_str();
    #endif
        ::write(fd, &ie,sizeof(ie));
    }

    void event_device::write(ushort type, ushort code, int value){
        this->write(create_event(type, code, value));
    }
    void event_device::ev_syn(){
        this->write(create_event(EV_SYN, SYN_REPORT, 0));
    }
    void event_device::ev_dropped(){
        this->write(create_event(EV_SYN, SYN_DROPPED, 0));
    }
}
