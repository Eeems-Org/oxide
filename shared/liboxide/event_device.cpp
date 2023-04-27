#include "event_device.h"
#include "debug.h"

namespace Oxide {
    input_event event_device::create_event(ushort type, ushort code, int value){
        struct input_event event;
        event.type = type;
        event.code = code;
        event.value = value;
        return event;
    }

    event_device::event_device(const string& path, int flags) : device(path), flags(flags){
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
        O_DEBUG("locking " << device.c_str());
        int result = ioctl(fd, EVIOCGRAB, 1);
        if(result == EBUSY){
            O_WARNING("Device is busy");
        }else if(result != 0){
            O_WARNING("Unknown error: " << result);
        }else{
            locked = true;
            O_DEBUG(device.c_str() << " locked");
        }
        return result;
    }

    int event_device::unlock(){
        int result = ioctl(fd, EVIOCGRAB, 0);
        if(result){
            O_WARNING("Failed to unlock " << device.c_str() << ": " << result);
        }else{
            locked = false;
            O_DEBUG("Unlocked " << device.c_str());
        }
        return result;
    }

    void event_device::write(input_event ie){
        if(fd <= 0){
            O_WARNING("Failed to write event to " << device.c_str() << ". Device not open.")
            return;
        }
        O_DEBUG("WRITE: " << ie.type << ", " << ie.code << ", " << ie.value << " to " << device.c_str());
        if(::write(fd, &ie,sizeof(ie)) < 0){
            O_WARNING("Failed to write to " << device.c_str() << ". " << strerror(errno))
        }
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
