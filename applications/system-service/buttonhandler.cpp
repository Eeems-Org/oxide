#include "buttonhandler.h"
#include "dbusservice.h"

void button_exit_handler(){
    // Release lock
    unlock_device(buttons);
    close(buttons.fd);
}

void flush_stream(istream* stream){
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));
    stream->read((char*)&ie, sie);
}
void press_button(event_device& evdev, int code, istream* stream){
    qDebug() << "inject button " << code;
    unlock_device(evdev);
    ev_key(evdev, code, 1);
    flush_stream(stream);
    ev_syn(evdev);
    flush_stream(stream);
    ev_key(evdev, code, 0);
    flush_stream(stream);
    ev_syn(evdev);
    flush_stream(stream);
    lock_device(evdev);
}

ButtonHandler* ButtonHandler::init(){
    static ButtonHandler* instance;
    if(instance != nullptr){
        return instance;
    }
    // Get event devices
    if(buttons.fd == -1){
        qDebug() << "Failed to open event device: " << buttons.device.c_str();
        throw QException();
    }
    if(atexit(button_exit_handler)){
        button_exit_handler();
        throw QException();
    }
    instance = new ButtonHandler();
    instance->start();
    return instance;
}

void ButtonHandler::run(){
    char name[256];
    memset(name, 0, sizeof(name));
    ioctl(buttons.fd, EVIOCGNAME(sizeof(name)), name);
    qDebug() << "Reading From : " << buttons.device.c_str() << " (" << name << ")";
    lock_device(buttons);
    qDebug() << "Registering exit handler...";
    // Mapping the correct button IDs.
    unordered_map<int, PressRecord> map;
    map[105] = PressRecord("Left", Qt::Key_Left);
    map[102] = PressRecord("Middle", Qt::Key_Home);
    map[106] = PressRecord("Right", Qt::Key_Right);
    map[116] = PressRecord("Power", Qt::Key_PowerOff);

    qDebug() << "Listening for keypresses...";
    // Get the size of an input event in the right format!
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));

    while(stream.read((char*)&ie, sie)){
        // TODO - Properly pass through non-button presses
        // Read for non-zero event codes.
        if(ie.code != 0){
            emit activity();
            // Toggle the button state.
            map[ie.code].pressed = !map[ie.code].pressed;
            if(!map[ie.code].pressed && map[ie.code].name == "Unknown"){
                press_button(buttons, ie.code, &stream);
            }else if(map[ie.code].pressed){
                emit keyDown(map[ie.code].keyCode);
            }else{
                emit keyUp(map[ie.code].keyCode);
            }
        }else{
            yieldCurrentThread();
        }
    }
}
void ButtonHandler::pressKey(Qt::Key key){
    int code;
    switch(key){
        case Qt::Key_Left:
            code = 105;
        break;
        case Qt::Key_Home:
            code = 102;
        break;
        case Qt::Key_Right:
            code = 106;
        break;
        case Qt::Key_PowerOff:
            code = 116;
        break;
        default:
            return;
    }
    press_button(buttons, code, &stream);
}
