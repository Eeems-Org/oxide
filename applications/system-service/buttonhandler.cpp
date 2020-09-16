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
void redraw_screen(int fd){
    mxcfb_update_data update_data;
    mxcfb_rect update_rect;
    update_rect.top = 0;
    update_rect.left = 0;
    update_rect.width = DISPLAYWIDTH;
    update_rect.height = DISPLAYHEIGHT;
    update_data.update_marker = 0;
    update_data.update_region = update_rect;
    update_data.waveform_mode = WAVEFORM_MODE_AUTO;
    update_data.update_mode = UPDATE_MODE_FULL;
    update_data.dither_mode = EPDC_FLAG_USE_DITHERING_MAX;
    update_data.temp = TEMP_USE_REMARKABLE_DRAW;
    update_data.flags = 0;
    ioctl(fd, MXCFB_SEND_UPDATE, &update_data);
}
void exit_handler(){
    // Release lock
    unlock_device(buttons);
    close(buttons.fd);
    close(touchScreen.fd);
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

ButtonHandler* ButtonHandler::init(QString ppid){
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
    instance = new ButtonHandler(ppid);
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
    map[105] = PressRecord("Left");
    map[102] = PressRecord("Middle");
    map[106] = PressRecord("Right");
    map[116] = PressRecord("Power");

    qDebug() << "Listening for keypresses...";
    // Get the size of an input event in the right format!
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));
    static char* privateBuffer = new char[DISPLAYSIZE];

    __gnu_cxx::stdio_filebuf<char> filebuf(buttons.fd, std::ios::in);
    istream stream(&filebuf);
    while(stream.read((char*)&ie, sie)){
        // TODO - Properly pass through non-button presses
        // Read for non-zero event codes.
        if(ie.code != 0){
            // Toggle the button state.
            map[ie.code].pressed = !map[ie.code].pressed;
            // On press
            if(map[ie.code].pressed){
                qDebug() << map[ie.code].name.c_str() << " DOWN";
                gettimeofday(&map[ie.code].pressTime,NULL);
            }else{
                struct timeval ctime;
                gettimeofday(&ctime,NULL);
                // Calculate length of hold
                long usecs = ((ctime.tv_sec   -  map[ie.code].pressTime.tv_sec  )*1000000L
                              +ctime.tv_usec) -  map[ie.code].pressTime.tv_usec;
                qDebug() << map[ie.code].name.c_str() << " UP (" << usecs << " microseconds)";
                if(usecs > 1000000L && map[ie.code].name == "Left"){
                    auto api = (AppsAPI*)DBusService::singleton()->getAPI("apps");
                    auto startupApplication = api->startupApplication();
                    auto currentApplication = api->getApplication(api->currentApplication());
                    if(currentApplication->state() == Application::Inactive|| currentApplication->path() != startupApplication.path()){
                        api->getApplication(startupApplication)->launch();
                    }
                }else if(usecs > 1000000L && map[ie.code].name == "Right"){
                    takeScreenshot();
                    if(QFile("/tmp/.screenshot").exists()){
                        ifstream screenshotfile;
                        // Then execute the contents of /tmp/.terminate
                        screenshotfile.open("/tmp/.screenshot", ios::in);
                        if(screenshotfile.is_open()){
                            qDebug() << "Screenshot file exists and can be read.";
                            screenshotfile.close();
                            system("/bin/bash /tmp/.screenshot");
                        }else{
                            qDebug() << "Screenshot file couldn't be read.";
                        }
                    }
                }else if(usecs > 1000000L && map[ie.code].name == "Middle"){
                    qDebug() << "Cloning screen...";
                    int frameBufferHandle = open("/dev/fb0", O_RDWR);
                    char* frameBuffer = (char*)mmap(0, DISPLAYSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, frameBufferHandle, 0);
                    memcpy(privateBuffer, frameBuffer, DISPLAYSIZE);
                    // TODO Show some sort of message on screen letting them know that the process manager is starting
                    vector<string> procs;
                    int i_ppid;
                    string my_pid;
                    if(!ppid.isEmpty()){
                        i_ppid = stoi(ppid.toStdString());
                        my_pid = to_string(getpid());
                        procs  = split_string_by_newline(exec((
                            "grep -Erl /proc/*/status --regexp='PPid:\\s+" + ppid + "' | awk '{print substr($1, 7, length($1) - 13)}'"
                        ).toStdString().c_str()));
                        qDebug() << "Pausing child tasks...";
                        for(auto pid : procs){
                          string cmd = "cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'";
                          if(my_pid != pid && is_uint(pid) && exec(cmd.c_str()) == ppid.toStdString() + "\n"){
                              qDebug() << "  " << pid.c_str();
                              // Found a child process
                              auto i_pid = stoi(pid);
                              // Pause the process
                              kill(i_pid, SIGSTOP);
                          }
                        }
                        kill(i_ppid, SIGSTOP);
                    }
                    qDebug() << "Running task manager.";
                    if(QFile("/opt/bin/erode").exists()){
                        system(("/opt/bin/erode " + ppid).toStdString().c_str());
                    }else if(QFile("/bin/erode").exists()){
                        system(("/bin/erode " + ppid).toStdString().c_str());
                    }else if(QFile("/usr/bin/erode").exists()){
                        system(("/usr/bin/erode " + ppid).toStdString().c_str());
                    }else{
                        qDebug() << "Could not find task manager.";
                    }
                    qDebug() << "Restoring screen...";
                    memcpy(frameBuffer, privateBuffer, DISPLAYSIZE);
                    redraw_screen(frameBufferHandle);
                    close(frameBufferHandle);
                    inputManager.clear_touch_buffer(touchScreen.fd);
                    if(!ppid.isEmpty()){
                        lock_device(touchScreen);
                        qDebug() << "Resuming child tasks...";
                        kill(i_ppid, SIGCONT);
                        for(auto pid : procs){
                          string cmd = "cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'";
                          if(my_pid != pid && is_uint(pid) && exec(cmd.c_str()) == ppid.toStdString() + "\n"){
                              qDebug() << "  " << pid.c_str();
                              // Found a child process
                              auto i_pid = stoi(pid);
                              // Pause the process
                              kill(i_pid, SIGCONT);
                          }
                        }
                        unlock_device(touchScreen);
                    }
                }else{
                    press_button(buttons, ie.code, &stream);
                }
            }
        }
    }
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
