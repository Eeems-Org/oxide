#include "buttonhandler.h"

#include "dbusservice.h"

using namespace Oxide;

void button_exit_handler(){
    // Release lock
    buttons.unlock();
    buttons.close();
}

void flush_stream(istream* stream){
    // Skip the next event
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));
    stream->read((char*)&ie, sie);
}
void press_button(event_device& evdev, int code, istream* stream){
    Q_UNUSED(stream)
#ifdef DEBUG
    O_INFO("inject button " << code);
#endif
//    evdev.unlock();
    evdev.write(EV_KEY, code, 1);
//    flush_stream(stream);
    evdev.ev_syn();
//    flush_stream(stream);
    evdev.write(EV_KEY, code, 0);
//    flush_stream(stream);
    evdev.ev_syn();
//    flush_stream(stream);
//    evdev.lock();
}

ButtonHandler* ButtonHandler::init(){
    static ButtonHandler* instance;
    if(instance != nullptr){
        return instance;
    }
    // Get event devices
    if(buttons.fd == -1){
        O_INFO("Failed to open event device: " << buttons.device.c_str());
        throw QException();
    }
    if(atexit(button_exit_handler)){
        button_exit_handler();
        throw QException();
    }
    instance = new ButtonHandler();
    return instance;
}

ButtonHandler::ButtonHandler() : QThread(), filebuf(buttons.fd, ios::in), stream(&filebuf), pressed(), timer(this), m_enabled(true) {
    timer.setInterval(100);
    timer.setSingleShot(false);
    connect(&timer, &QTimer::timeout, this, &ButtonHandler::timeout);
    setObjectName("buttons");
    moveToThread(this);
    Oxide::startThreadWithPriority(this, QThread::HighestPriority);
}

void ButtonHandler::setEnabled(bool enabled){ m_enabled = enabled; }

void ButtonHandler::run(){
    char name[256];
    memset(name, 0, sizeof(name));
    ioctl(buttons.fd, EVIOCGNAME(sizeof(name)), name);
    O_INFO("Reading From : " << buttons.device.c_str() << " (" << name << ")");
    //buttons.lock();
    O_INFO("Registering exit handler...");
    // Mapping the correct button IDs.
    unordered_map<int, PressRecord> map;
    map[105] = PressRecord("Left", Qt::Key_Left);
    map[102] = PressRecord("Middle", Qt::Key_Home);
    map[106] = PressRecord("Right", Qt::Key_Right);
    map[116] = PressRecord("Power", Qt::Key_PowerOff);

    O_INFO("Listening for keypresses...");
    // Get the size of an input event in the right format!
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));

    while(stream.read((char*)&ie, sie)){
        // TODO - Properly pass through non-button presses
        // Read for non-zero event codes.
        // TODO - buffer events until we are okay with sending them
        emit rawEvent(ie);
        if(ie.code != 0){
            emit activity();
            // Toggle the button state.
            map[ie.code].pressed = !map[ie.code].pressed;
            if(!map[ie.code].pressed && map[ie.code].name == "Unknown"){
                //press_button(buttons, ie.code, &stream);
            }else if(map[ie.code].pressed){
                emit keyDown(map[ie.code].keyCode);
            }else{
                emit keyUp(map[ie.code].keyCode);
            }
        }else{
            qApp->processEvents(QEventLoop::AllEvents, 100);
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

void ButtonHandler::keyDown(Qt::Key key){
    if(!m_enabled){
        return;
    }
    O_INFO("Down" << key);
    if(validKeys.contains(key) && !pressed.contains(key)){
        QElapsedTimer timer;
        timer.start();
        pressed.insert(key, timer);
    }
}

void ButtonHandler::keyUp(Qt::Key key){
    if(!m_enabled){
        return;
    }
    O_INFO("Up" << key);
    if(!pressed.contains(key)){
        // This should never happen
        return;
    }
    auto value = pressed.value(key);
    pressed.remove(key);
    if(value.hasExpired(700)){
        // Held event already fired
        return;
    }
    if(key == Qt::Key_PowerOff){
        emit powerPress();
        return;
    }
    //        pressKey(key);
}

void ButtonHandler::timeout(){
    if(!m_enabled){
        return;
    }
    for(auto key : pressed.keys()){
        // If the key has been held for a while
        if(!pressed.value(key).hasExpired(700)){
            continue;
        }
        O_INFO("Key held" << key);
        switch(key){
            case Qt::Key_Left:
                emit leftHeld();
                break;
            case Qt::Key_Home:
                emit homeHeld();
                break;
            case Qt::Key_Right:
                emit rightHeld();
                break;
            case Qt::Key_PowerOff:
                emit powerHeld();
                break;
            default:
                continue;
        }
        pressed.remove(key);
    }
}
