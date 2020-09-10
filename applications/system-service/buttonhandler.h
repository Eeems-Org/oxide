#ifndef BUTTONHANDLER_H
#define BUTTONHANDLER_H

#include <QThread>
#include <QFile>
#include <QException>
#include <QDebug>

#include <string>
#include <iostream>
#include <unordered_map>
#include <ext/stdio_filebuf.h>
#include <linux/input.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <cstdlib>

#include "fb2png.h"
#include "mxcfb.h"
#include "inputmanager.h"

using namespace std;

#define PNG_PATH "/tmp/fb.png"
#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872
#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define DISPLAYSIZE DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(uint16_t)

struct PressRecord {
    bool pressed = false;
    struct timeval pressTime;
    string name = "Unknown";
    PressRecord (string name) : name(name) {}
    PressRecord(){}
};

struct event_device {
    string device;
    int fd;
    event_device(string path, int flags){
        device = path;
        fd = open(path.c_str(), flags);
    }
};

// const event_device wacom("/dev/input/event0", O_WRONLY);
const event_device buttons("/dev/input/event2", O_RDWR);
const event_device touchScreen("/dev/input/event1", O_WRONLY);

class ButtonHandler : public QThread {
    Q_OBJECT

public:
    static ButtonHandler* init(QString ppid);
    static string exec(const char* cmd);
    static vector<std::string> split_string_by_newline(const std::string& str);
    static int is_uint(string input);

    ButtonHandler(QString ppid) : QThread(), ppid(ppid), inputManager() {}

protected:
    void run();
    QString ppid;
    InputManager inputManager;
};

#endif // BUTTONHANDLER_H
