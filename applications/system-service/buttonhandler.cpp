#include "buttonhandler.h"


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
#ifdef DEBUG
    qDebug() << "inject button " << code;
#endif
    evdev.unlock();
    evdev.write(EV_KEY, code, 1);
    flush_stream(stream);
    evdev.ev_syn();
    flush_stream(stream);
    evdev.write(EV_KEY, code, 0);
    flush_stream(stream);
    evdev.ev_syn();
    flush_stream(stream);
    evdev.lock();
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

ButtonHandler::ButtonHandler() : QThread(), filebuf(buttons.fd, ios::in), stream(&filebuf), pressed(), timer(this), m_enabled(true) {
    flood = build_flood();
    timer.setInterval(100);
    timer.setSingleShot(false);
    connect(&timer, &QTimer::timeout, this, &ButtonHandler::timeout);
    timer.start();
}

void ButtonHandler::setEnabled(bool enabled){
    m_enabled = enabled;
}

void ButtonHandler::clear_buffer(){
    if(buttons.fd == -1){
        return;
    }
#ifdef DEBUG
    qDebug() << "Clearing event buffer on" << buttons.device.c_str();
#endif
    ::write(buttons.fd, flood, EVENT_FLOOD_SIZE);
}

input_event* ButtonHandler::event_flood(){ return flood; }

void ButtonHandler::run(){
    char name[256];
    memset(name, 0, sizeof(name));
    ioctl(buttons.fd, EVIOCGNAME(sizeof(name)), name);
    qDebug() << "Reading From : " << buttons.device.c_str() << " (" << name << ")";
    buttons.lock();
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
        emit rawEvent(ie);
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
            qApp->processEvents(QEventLoop::AllEvents, 100);
            yieldCurrentThread();
        }
    }
}

input_event* ButtonHandler::build_flood(){
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

input_event ButtonHandler::createEvent(ushort type, ushort code, int value){
    struct input_event event;
    event.type = type;
    event.code = code;
    event.value = value;
    return event;
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
    qDebug() << "Down" << key;
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
    qDebug() << "Up" << key;
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
    pressKey(key);
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
        qDebug() << "Key held" << key;
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

#include "moc_buttonhandler.cpp"
