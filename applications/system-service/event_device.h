#ifndef EVENT_DEVICE_H
#define EVENT_DEVICE_H

#include <QDebug>

#include <string>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

using namespace std;

struct event_device {
    string device;
    int fd;
    bool locked = false;
    event_device(string path, int flags){
        device = path;
        fd = open(path.c_str(), flags);
    }
};

int lock_device(event_device& evdev);
int unlock_device(event_device& evdev);
void write_event(event_device& evdev, input_event ie);
void ev_syn(event_device& evdev);
void ev_key(event_device& evdev, int code, int value = 0);

#endif // EVENT_DEVICE_H
