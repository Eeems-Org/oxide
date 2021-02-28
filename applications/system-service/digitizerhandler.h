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

#include "event_device.h"
#include "devicesettings.h"

using namespace std;

#define touchHandler DigitizerHandler::singleton_touchScreen()
#define wacomHandler DigitizerHandler::singleton_wacom()

class DigitizerHandler : public QThread {
    Q_OBJECT
public:
    static DigitizerHandler* singleton_touchScreen(){
        static DigitizerHandler* instance;
        if(instance != nullptr){
            return instance;
        }
        // Get event devices
        event_device touchScreen_device(deviceSettings.getTouchDevicePath(), O_RDONLY);
        if(touchScreen_device.fd == -1){
            qDebug() << "Failed to open event device: " << touchScreen_device.device.c_str();
            throw QException();
        }
        instance = new DigitizerHandler(touchScreen_device);
        instance->start();
        return instance;
    }
    static DigitizerHandler* singleton_wacom(){
        static DigitizerHandler* instance;
        if(instance != nullptr){
            return instance;
        }
        // Get event devices
        event_device wacom_device(deviceSettings.getWacomDevicePath(), O_RDONLY);
        if(wacom_device.fd == -1){
            qDebug() << "Failed to open event device: " << wacom_device.device.c_str();
            throw QException();
        }
        instance = new DigitizerHandler(wacom_device);
        instance->start();
        return instance;
    }
    static string exec(const char* cmd);
    static vector<std::string> split_string_by_newline(const std::string& str);
    static int is_uint(string input);

    DigitizerHandler(event_device& device)
     : QThread(),
       filebuf(device.fd, ios::in),
       stream(&filebuf),
       m_enabled(true),
       device(device) {
        flood = build_flood();
    }
    ~DigitizerHandler(){
        if(device.fd == -1){
            return;
        }
        if(device.locked){
            ungrab();
        }
        close(device.fd);
    }
    void setEnabled(bool enabled){
        m_enabled = enabled;
    }
    void grab(){
        if(!grabbed()){
            lock_device(device);
        }
    }
    void ungrab(){
        if(grabbed()){
            unlock_device(device);
        }
    }
    bool grabbed() { return device.locked; }
    void write(ushort type, ushort code, int value = 0){
        write_event(device, createEvent(type, code, value));
        qDebug() << "Emitted event " << type << code << value;
    }
    void syn(){
        write(EV_SYN, SYN_REPORT);
    }
    void clear_buffer(){
        if(device.fd == -1){
            return;
        }
#ifdef DEBUG
        qDebug() << "Clearing event buffer on" << device.device.c_str();
#endif
        ::write(device.fd, flood, 512 * 8 * 4 * sizeof(input_event));
    }

signals:
    void activity();
    void inputEvent(const input_event& event);

protected:
    input_event* flood;
    input_event* build_flood(){
        auto n = 512 * 8;
        auto num_inst = 4;
        input_event* ev = (input_event *)malloc(sizeof(struct input_event) * n * num_inst);
        memset(ev, 0, sizeof(input_event) * n * num_inst);
        auto i = 0;
        while (i < n) {
            ev[i++] = createEvent(EV_ABS, ABS_DISTANCE, 1);
            ev[i++] = createEvent(EV_SYN, 0, 0);
            ev[i++] = createEvent(EV_ABS, ABS_DISTANCE, 2);
            ev[i++] = createEvent(EV_SYN, 0, 0);
        }
        return ev;
    }
    void run(){
        char name[256];
        memset(name, 0, sizeof(name));
        ioctl(device.fd, EVIOCGNAME(sizeof(name)), name);
        qDebug() << "Reading From : " << device.device.c_str() << " (" << name << ")";
        qDebug() << "Listening for events...";
        while(handle_events()){
            yieldCurrentThread();
        }
    }
    bool handle_events(){
        input_event ie;
        if(!read(&ie)){
            return false;
        }
        emit inputEvent(ie);
        emit activity();
        return true;
    }
    __gnu_cxx::stdio_filebuf<char> filebuf;
    istream stream;
    bool m_enabled;
    event_device device;
    bool read(input_event* ie){
        return (bool)stream.read((char*)ie, static_cast<streamsize>(sizeof(struct input_event)));
    }
    void flush_stream(){
        input_event ie;
        read(&ie);
    }
    inline input_event createEvent(ushort type, ushort code, int value){
        struct input_event event;
        event.type = type;
        event.code = code;
        event.value = value;
        return event;
    }
};

#endif // DIGITIZERHANDLER_H
