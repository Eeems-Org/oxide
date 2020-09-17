#include "buttonhandler.h"
#include "dbusservice.h"

void removeScreenshot(){
    QFile file(PNG_PATH);
    if(file.exists()){
        qDebug() << "Removing framebuffer image";
        file.remove();
    }
}
void takeScreenshot(){
    qDebug() << "Taking screenshot";
    int res = fb2png_defaults();
    if(res){
        qDebug() << "Failed to take screenshot: " << res;
    }
}
int lock_device(event_device evdev){
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
int unlock_device(event_device evdev){
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
void write_event(event_device evdev, input_event ie){
    qDebug() << "WRITE: " << ie.type << ", " << ie.code << ", " << ie.value << " to " << evdev.device.c_str();
    write(evdev.fd, &ie,sizeof(ie));
}
void flush_stream(istream* stream){
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));
    stream->read((char*)&ie, sie);
}
void ev_syn(event_device evdev){
    struct input_event key_input_event;
    key_input_event.type = EV_SYN;
    key_input_event.code = SYN_REPORT;
    write_event(evdev, key_input_event);
}
void ev_key(event_device evdev, int code, int value = 0){
    struct input_event key_input_event;
    key_input_event.type = EV_KEY;
    key_input_event.code = code;
    key_input_event.value = value;
    write_event(evdev, key_input_event);
}
void press_button(event_device evdev, int code, istream* stream){
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
        throw QException();
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
            // On press
//            if(map[ie.code].pressed){
//                qDebug() << map[ie.code].name.c_str() << " DOWN";
//                gettimeofday(&map[ie.code].pressTime,NULL);
//            }else{
//                struct timeval ctime;
//                gettimeofday(&ctime,NULL);
//                // Calculate length of hold
//                long usecs = ((ctime.tv_sec   -  map[ie.code].pressTime.tv_sec  )*1000000L
//                              +ctime.tv_usec) -  map[ie.code].pressTime.tv_usec;
//                qDebug() << map[ie.code].name.c_str() << " UP (" << usecs << " microseconds)";
//                if(usecs > 1000000L && map[ie.code].name == "Left"){
//                    auto api = (AppsAPI*)DBusService::singleton()->getAPI("apps");
//                    auto startupApplication = api->startupApplication();
//                    auto currentApplication = api->getApplication(api->currentApplication());
//                    if(currentApplication->state() == Application::Inactive|| currentApplication->path() != startupApplication.path()){
//                        api->getApplication(startupApplication)->launch();
//                    }
//                }else if(usecs > 1000000L && map[ie.code].name == "Right"){
//                    takeScreenshot();
//                    if(QFile("/tmp/.screenshot").exists()){
//                        ifstream screenshotfile;
//                        // Then execute the contents of /tmp/.terminate
//                        screenshotfile.open("/tmp/.screenshot", ios::in);
//                        if(screenshotfile.is_open()){
//                            qDebug() << "Screenshot file exists and can be read.";
//                            screenshotfile.close();
//                            system("/bin/bash /tmp/.screenshot");
//                        }else{
//                            qDebug() << "Screenshot file couldn't be read.";
//                        }
//                    }
//                }else if(usecs > 1000000L && map[ie.code].name == "Middle"){
//                    auto api = (AppsAPI*)DBusService::singleton()->getAPI("apps");
//                    auto applications = api->getApplications();
//                    auto app = api->getApplication("Process Manager");
//                    if(app == nullptr){
//                        qDebug() << "Can't find process manager";
//                        continue;
//                    }
//                    if(api->currentApplication().path() == app->path()){
//                        qDebug() << "Process manager is active";
//                        continue;
//                    }
//                    app->launch();
//                }else{
//                    press_button(buttons, ie.code, &stream);
//                }
//            }
//        }
//    }
}
void ButtonHandler::pressKey(Qt::Key key){
    press_button(buttons, key, &stream);
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
