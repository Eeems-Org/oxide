#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <iostream>
#include <vector>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>

using namespace std;

inline input_event createEvent(ushort type, ushort code, int value){
    struct input_event event;
    event.type = type;
    event.code = code;
    event.value = value;
    return event;
}

class InputManager {
public:
    input_event *touch_flood;
    input_event *button_flood;
    InputManager() {
    this->touch_flood = build_touch_flood();
    this->button_flood = build_button_flood();
    }

    input_event *build_touch_flood() {
        auto n = 512 * 8;
        auto num_inst = 4;
        input_event *ev = (input_event *)malloc(sizeof(struct input_event) * n * num_inst);
        memset(ev, 0, sizeof(input_event) * n * num_inst);

        auto i = 0;
        while (i < n) {
            ev[i++] = createEvent(EV_ABS, ABS_DISTANCE, 1);
            ev[i++] = createEvent(EV_SYN, 0, 0);
            ev[i++] = createEvent(EV_ABS, ABS_DISTANCE, 0);
            ev[i++] = createEvent(EV_SYN, 0, 0);
        }

        return ev;
    }

    void clear_touch_buffer(int fd) {
        write(fd, touch_flood, 512 * 8 * 4 * sizeof(input_event));
    }

    input_event *build_button_flood() {
        auto n = 512 * 8;
        auto num_inst = 2;
        input_event *ev = (input_event *)malloc(sizeof(struct input_event) * n * num_inst);
        memset(ev, 0, sizeof(input_event) * n * num_inst);

        auto i = 0;
        while (i < n) {
            ev[i++] = createEvent(EV_SYN, 1, 0);
            ev[i++] = createEvent(EV_SYN, 0, 1);
        }

        return ev;
    }

    void clear_button_buffer(int fd) {
        write(fd, button_flood, 512 * 8 * 2 * sizeof(input_event));
    }
};


#endif // INPUTMANAGER_H
