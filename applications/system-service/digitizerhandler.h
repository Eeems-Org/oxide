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
#include "settings.h"

using namespace std;

#define touchHandler DigitizerHandler::singleton_touchScreen()
#define wacomHandler DigitizerHandler::singleton_wacom()

const event_device wacom_device(Settings::instance().getWacomDevicePath(), O_RDONLY);
const event_device touchScreen_device(Settings::instance().getTouchDevicePath(), O_RDONLY);

class DigitizerHandler : public QThread {
    Q_OBJECT
public:
    static DigitizerHandler* singleton_touchScreen(){
        static DigitizerHandler* instance;
        if(instance != nullptr){
            return instance;
        }
        // Get event devices
        if(touchScreen_device.fd == -1){
            qDebug() << "Failed to open event device: " << touchScreen_device.device.c_str();
            throw QException();
        }
        if(atexit(touch_exit_handler)){
            touch_exit_handler();
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
        if(wacom_device.fd == -1){
            qDebug() << "Failed to open event device: " << wacom_device.device.c_str();
            throw QException();
        }
        if(atexit(wacom_exit_handler)){
            wacom_exit_handler();
            throw QException();
        }
        instance = new DigitizerHandler(wacom_device);
        instance->start();
        return instance;
    }
    static string exec(const char* cmd);
    static vector<std::string> split_string_by_newline(const std::string& str);
    static int is_uint(string input);

    DigitizerHandler(const event_device& device)
     : QThread(),
       filebuf(device.fd, ios::in),
       stream(&filebuf),
       m_enabled(true),
       device(device) {}
    void setEnabled(bool enabled){
        m_enabled = enabled;
    }

signals:
    void activity();

protected:
    void run(){
        char name[256];
        memset(name, 0, sizeof(name));
        ioctl(device.fd, EVIOCGNAME(sizeof(name)), name);
        qDebug() << "Reading From : " << device.device.c_str() << " (" << name << ")";
        qDebug() << "Listening for events...";
        // Get the size of an input event in the right format!
        input_event ie;
        streamsize sie = static_cast<streamsize>(sizeof(struct input_event));
        while(stream.read((char*)&ie, sie)){
            // Read for non-zero event codes.
            if(ie.code != 0){
                emit activity();
            }else{
                yieldCurrentThread();
            }
        }
    }
    __gnu_cxx::stdio_filebuf<char> filebuf;
    istream stream;
    bool m_enabled;
    const event_device& device;
    static void touch_exit_handler(){
        if(touchScreen_device.fd != -1){
            // unlock_device(touchScreen_device);
            close(touchScreen_device.fd);
        }
    }
    static void wacom_exit_handler(){
        if(wacom_device.fd != -1){
            // unlock_device(wacom_device);
            close(wacom_device.fd);
        }
    }
};

#endif // DIGITIZERHANDLER_H
