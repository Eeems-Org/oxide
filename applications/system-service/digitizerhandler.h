#pragma once
#ifndef DIGITIZERHANDLER_H
#define DIGITIZERHANDLER_H

#include <QThread>
#include <QException>

#include <ext/stdio_filebuf.h>
#include <sstream>
#include <linux/input.h>
#include <iostream>
#include <string>
#include <vector>
#include <liboxide.h>
#include <liboxide/event_device.h>

using namespace std;
using namespace Oxide;

#define touchHandler DigitizerHandler::singleton_touchScreen()

class DigitizerHandler : public QThread {
    Q_OBJECT

public:
    static DigitizerHandler* singleton_touchScreen(){
        static DigitizerHandler* instance;
        if(instance != nullptr){
            return instance;
        }
        // Get event devices
        event_device touchScreen_device(deviceSettings.getTouchDevicePath(), O_RDWR);
        if(touchScreen_device.fd == -1){
            O_WARNING("Failed to open event device: " << touchScreen_device.device.c_str());

        }
        instance = new DigitizerHandler(touchScreen_device, "touchscreen");
        return instance;
    }
    static string exec(const char* cmd);
    static vector<std::string> split_string_by_newline(const std::string& str);
    static int is_uint(string input);

    DigitizerHandler(event_device& device, QString name)
     : QThread(),
       filebuf(device.fd, ios::in),
       stream(&filebuf),
       m_enabled(true),
       device(device)
    {
        setObjectName(name);
        moveToThread(this);
        Oxide::startThreadWithPriority(this, QThread::HighestPriority);
    }
    ~DigitizerHandler(){
        if(device.fd == -1){
            return;
        }
        close(device.fd);
    }
    void setEnabled(bool enabled){ m_enabled = enabled; }
    void write(ushort type, ushort code, int value){
        if(device.fd == -1){
            return;
        }
        auto event = createEvent(type, code, value);
        ::write(device.fd, &event, sizeof(input_event));
        O_DEBUG("Emitted event " << event.input_event_sec << event.input_event_usec << type << code << value);
    }
    void write(input_event* events, size_t size){
        if(device.fd == -1){
            return;
        }
        ::write(device.fd, events, size);
    }
    void syn(){
        write(EV_SYN, SYN_REPORT, 0);
    }
    static inline input_event createEvent(ushort type, ushort code, int value){
        struct input_event event;
        event.type = type;
        event.code = code;
        event.value = value;
        return event;
    }

signals:
    void activity();
    void inputEvent(const input_event& event);

protected:
    void run(){
        if(device.fd == -1){
            O_WARNING("No device, not starting for thread");
            return;
        }
        char name[256];
        memset(name, 0, sizeof(name));
        ioctl(device.fd, EVIOCGNAME(sizeof(name)), name);
        O_DEBUG("Reading From : " << device.device.c_str() << " (" << name << ")");
        O_DEBUG("Listening for events...");
        while(handle_events()){
            qApp->processEvents(QEventLoop::AllEvents, 100);
            QThread::yieldCurrentThread();
        }
    }
    bool handle_events(){
        vector<input_event> event_buffer;
        bool success = true;
        while(true){
            input_event event;
            if(!read(&event)){
                success = false;
                goto exitLoop;
            }
            event_buffer.push_back(event);
            switch(event.type){
                case EV_SYN:
                    switch(event.code){
                        case SYN_DROPPED:
                            event_buffer.clear();
                            skip_event();
                        case SYN_REPORT:
                        case SYN_MT_REPORT:
                            goto exitLoop;
                        break;
                    }
                break;
            }
        }
        exitLoop:
        if(event_buffer.size()){
            for(input_event event : event_buffer){
                emit inputEvent(event);
            }
            emit activity();
        }
        return success;
    }
    void skip_event(){
        input_event event;
        while(read(&event)){
            if(event.type != EV_SYN){
                continue;
            }
            if(event.code == SYN_REPORT || event.code == SYN_MT_REPORT){
                break;
            }
        }
    }
    __gnu_cxx::stdio_filebuf<char> filebuf;
    istream stream;
    bool m_enabled;
    event_device device;
    bool read(input_event* ie){
        return (bool)stream.read((char*)ie, static_cast<streamsize>(sizeof(struct input_event)));
    }
};

#endif // DIGITIZERHANDLER_H
