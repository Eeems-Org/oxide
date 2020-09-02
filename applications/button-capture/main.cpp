#include <iostream>
#include <linux/input.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <sys/time.h>
#include <ext/stdio_filebuf.h>
#include "fb2png.h"
#include <sstream>
#include <memory>
#include <experimental/filesystem>
#include <signal.h>
#include <dirent.h>

using namespace std;

const char* pngpath = "/tmp/fb.png";

bool exists(const std::string& name) {
    std::fstream file(name.c_str());
    return file.good();
}

void removeScreenshot(){
    if(exists(pngpath)){
        cout << "Removing framebuffer image" << endl;
        remove(pngpath);
    }
}
void takeScreenshot(){
    cout << "Taking screenshot" << endl;
    int res = fb2png_defaults();
    if(res){
        cout << "Failed to take screenshot: " << res << endl;
    }
}

//Keeping track of presses.
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

const event_device evdev("/dev/input/event2", O_RDWR);


int lock_device(event_device evdev){
    cout << "locking " << evdev.device << endl;
    int result = ioctl(evdev.fd, EVIOCGRAB, 1);
    if(result == EBUSY){
        cout << "Device is busy" << endl;
    }else if(result != 0){
        cout << "Unknown error: " << result << endl;
    }else{
        cout << evdev.device << " locked" << endl;
    }
    return result;
}
int unlock_device(event_device evdev){
    int result = ioctl(evdev.fd, EVIOCGRAB, 0);
    if(result){
        cout << "Failed to unlock " << evdev.device << ": " << result << endl;
    }else{
        cout << "Unlocked " << evdev.device << endl;
    }
    return result;
}
void exit_handler(){
    // Release lock
    unlock_device(evdev);
    close(evdev.fd);
}
void write_event(event_device evdev, input_event ie){
    cout << "WRITE: " << ie.type << ", " << ie.code << ", " << ie.value << " to " << evdev.device << endl;
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
    cout << "inject button " << code << endl;
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


string exec(const char* cmd) {
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
int is_uint(string input){
    unsigned int i;
    for (i=0; i < input.length(); i++){
        if(!isdigit(input.at(i))){
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]){
    // Mapping the correct button IDs.
    unordered_map<int, PressRecord> map;
    map[105] = PressRecord("Left");
    map[102] = PressRecord("Middle");
    map[106] = PressRecord("Right");
    map[116] = PressRecord("Power");

    // Get event devices
    if(evdev.fd == -1){
        cout << "Failed to open event device: " << evdev.device << endl;
        return EXIT_FAILURE;
    }
    // Aquire lock
    char name[256];
    int result = 0;
    memset(name, 0, sizeof(name));
    result = ioctl(evdev.fd, EVIOCGNAME(sizeof(name)), name);
    cout << "Reading From : " << evdev.device << " (" << name << ")" << endl;
    lock_device(evdev);
    cout << "Registering exit handler..." << endl;
    result = atexit(exit_handler);
    if(result != 0){
        exit_handler();
        cout << "Unable to register exit handler" << endl;
        return result;
    }
    cout << "Listening for keypresses..." << endl;
    // Get the size of an input event in the right format!
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));

    __gnu_cxx::stdio_filebuf<char> filebuf(evdev.fd, std::ios::in);
    istream stream(&filebuf);
    while(stream.read((char*)&ie, sie)){
        // TODO - Properly pass through non-button presses
        // Read for non-zero event codes.
        if(ie.code != 0){
            // Toggle the button state.
            map[ie.code].pressed = !map[ie.code].pressed;
            // On press
            if(map[ie.code].pressed){
                cout << map[ie.code].name << " DOWN" << endl;
                gettimeofday(&map[ie.code].pressTime,NULL);
            }else{
                struct timeval ctime;
                gettimeofday(&ctime,NULL);
                // Calculate length of hold
                long usecs = ((ctime.tv_sec   -  map[ie.code].pressTime.tv_sec  )*1000000L
                              +ctime.tv_usec) -  map[ie.code].pressTime.tv_usec;
                cout << map[ie.code].name << " UP (" << usecs << " microseconds)" << endl;
                if(usecs > 1000000L && map[ie.code].name == "Left"){
                    if(exists("/tmp/.terminate")){
                        ifstream termfile;
                        // Then execute the contents of /tmp/.terminate
                        termfile.open("/tmp/.terminate", ios::in);
                        if(termfile.is_open()){
                            cout << "Termfile exists and can be read." << endl;
                            termfile.close();
                            system("/bin/bash /tmp/.terminate");
                        }else{
                            cout << "Termfile couldn't be read." << endl;
                        }
                    }
                }else if(usecs > 1000000L && map[ie.code].name == "Right"){
                    removeScreenshot();
                    takeScreenshot();
                    if(exists("/tmp/.screenshot")){
                        ifstream screenshotfile;
                        // Then execute the contents of /tmp/.terminate
                        screenshotfile.open("/tmp/.screenshot", ios::in);
                        if(screenshotfile.is_open()){
                            cout << "Screenshot file exists and can be read." << endl;
                            screenshotfile.close();
                            system("/bin/bash /tmp/.screenshot");
                        }else{
                            cout << "Screenshot file couldn't be read." << endl;
                        }
                    }
                }else if(argc > 1 && usecs > 1000000L && map[ie.code].name == "Middle"){
                    string ppid = argv[1];
                    auto i_ppid = stoi(ppid);
                    string my_pid = to_string(getpid());
                    auto path = "/proc/" + ppid + "/status";
                    auto procDir = opendir("/proc");
                    if(procDir != NULL){
                        cout << "Pausing child tasks..." << endl;
                        while(auto entry = readdir(procDir)){
                          string pid = entry->d_name;
                          if(my_pid != pid && is_uint(pid) && exec(("cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'").c_str()) == ppid + "\n"){
                              cout << "  " << pid << endl;
                              // Found a child process
                              auto i_pid = stoi(pid);
                              // Pause the process
                              kill(i_pid, SIGSTOP);
                          }
                        }
                    }
                    closedir(procDir);
                    kill(i_ppid, SIGSTOP);
                    cout << "Running task manager." << endl;
                    // Todo - record screenshot
                    if(exists("/opt/bin/erode")){
                        system("/opt/bin/erode");
                    }else if(exists("/bin/erode")){
                        system("/bin/erode");
                    }else if(exists("/usr/bin/erode")){
                        system("/usr/bin/erode");
                    }else{
                        cout << "Could not find task manager." << endl;
                    }
                    // Todo - redraw screenshot
                    // Todo - flush out touchscreen events from the buffer
                    kill(i_ppid, SIGCONT);
                    procDir = opendir("/proc");
                    if(procDir != NULL){
                        cout << "Resuming child tasks..." << endl;
                        while(auto entry = readdir(procDir)){
                          string pid = entry->d_name;
                          if(my_pid != pid && is_uint(pid) && exec(("cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'").c_str()) == ppid + "\n"){
                              cout << "  " << pid << endl;
                              // Found a child process
                              auto i_pid = stoi(pid);
                              // Resume the process
                              kill(i_pid, SIGCONT);
                          }
                        }
                    }
                    closedir(procDir);
                }else{
                    press_button(evdev, ie.code, &stream);
                }
            }
        }
    }
    cout << "Quitting..." << endl;
    return EXIT_SUCCESS;
}
