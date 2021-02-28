#include "event_device.h"

int lock_device(event_device& evdev){
    qDebug() << "locking " << evdev.device.c_str();
    int result = ioctl(evdev.fd, EVIOCGRAB, 1);
    if(result == EBUSY){
        qDebug() << "Device is busy";
    }else if(result != 0){
        qDebug() << "Unknown error: " << result;
    }else{
        evdev.locked = true;
        qDebug() << evdev.device.c_str() << " locked";
    }
    return result;
}
int unlock_device(event_device& evdev){
    int result = ioctl(evdev.fd, EVIOCGRAB, 0);
    if(result){
        qDebug() << "Failed to unlock " << evdev.device.c_str() << ": " << result;
    }else{
        evdev.locked = false;
        qDebug() << "Unlocked " << evdev.device.c_str();
    }
    return result;
}
void write_event(event_device& evdev, input_event ie){
#ifdef DEBUG
    qDebug() << "WRITE: " << ie.type << ", " << ie.code << ", " << ie.value << " to " << evdev.device.c_str();
#endif
    write(evdev.fd, &ie,sizeof(ie));
}
void ev_syn(event_device& evdev){
    struct input_event key_input_event;
    key_input_event.type = EV_SYN;
    key_input_event.code = SYN_REPORT;
    write_event(evdev, key_input_event);
}
void ev_key(event_device& evdev, int code, int value){
    struct input_event key_input_event;
    key_input_event.type = EV_KEY;
    key_input_event.code = code;
    key_input_event.value = value;
    write_event(evdev, key_input_event);
}
