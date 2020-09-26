#ifndef EVENT_DEVICE_H
#define EVENT_DEVICE_H

#include <QDebug>

#include <string>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

//#define DEBUG

using namespace std;

struct event_device {
    string device;
    int fd;
    event_device(string path, int flags){
        device = path;
        fd = open(path.c_str(), flags);
    }
};

int lock_device(const event_device& evdev);
int unlock_device(const event_device& evdev);
void write_event(const event_device& evdev, input_event ie);
void ev_syn(const event_device& evdev);
void ev_key(const event_device& evdev, int code, int value = 0);

#endif // EVENT_DEVICE_H
