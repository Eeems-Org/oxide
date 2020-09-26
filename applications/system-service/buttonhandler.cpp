#include "buttonhandler.h"
#include "dbusservice.h"

//#define DEBUG

int lock_device(const event_device& evdev){
    qDebug() << "locking " << evdev.device.c_str();
    int result = ioctl(evdev.fd, EVIOCGRAB, 1);
    if(result == EBUSY){
        qDebug() << "Device is busy";
    }else if(result != 0){
        qDebug() << "Unknown error: " << result;
    }else{
        qDebug() << evdev.device.c_str() << " locked";
    }
    return result;
}
int unlock_device(const event_device& evdev){
    int result = ioctl(evdev.fd, EVIOCGRAB, 0);
    if(result){
        qDebug() << "Failed to unlock " << evdev.device.c_str() << ": " << result;
    }else{
        qDebug() << "Unlocked " << evdev.device.c_str();
    }
    return result;
}
void exit_handler(){
    // Release lock
    unlock_device(buttons);
    close(buttons.fd);
//    close(touchScreen.fd);
    // close(wacom.fd);
}
void write_event(const event_device& evdev, input_event ie){
#ifdef DEBUG
    qDebug() << "WRITE: " << ie.type << ", " << ie.code << ", " << ie.value << " to " << evdev.device.c_str();
#endif
    write(evdev.fd, &ie,sizeof(ie));
}
void flush_stream(istream* stream){
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));
    stream->read((char*)&ie, sie);
}
void ev_syn(const event_device& evdev){
    struct input_event key_input_event;
    key_input_event.type = EV_SYN;
    key_input_event.code = SYN_REPORT;
    write_event(evdev, key_input_event);
}
void ev_key(const event_device& evdev, int code, int value = 0){
    struct input_event key_input_event;
    key_input_event.type = EV_KEY;
    key_input_event.code = code;
    key_input_event.value = value;
    write_event(evdev, key_input_event);
}
void press_button(const event_device& evdev, int code, istream* stream){
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
    if(atexit(exit_handler)){
        exit_handler();
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
string ButtonHandler::exec(const char* cmd){
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
vector<std::string> ButtonHandler::split_string_by_newline(const std::string& str){
    auto result = vector<std::string>{};
    auto ss = stringstream{str};

    for (string line; getline(ss, line, '\n');)
        result.push_back(line);

    return result;
}
int ButtonHandler::is_uint(string input){
    unsigned int i;
    for (i=0; i < input.length(); i++){
        if(!isdigit(input.at(i))){
            return 0;
        }
    }
    return 1;
}
