#include <iostream>
#include <linux/input.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <sys/time.h>

using namespace std;

//Keeping track of presses.
struct PressRecord {
    bool pressed = false;
    struct timeval pressTime;
    string name = "Unknown";
    PressRecord (string name) : name(name) {}
    PressRecord(){}
};

int main()
{
    // Mapping the correct button IDs.
    unordered_map<int, PressRecord> map;
    map[105] = PressRecord("Left");
    map[102] = PressRecord("Middle");
    map[106] = PressRecord("Right");
    map[116] = PressRecord("Power");

    // Open the button device.
    ifstream eventsfile;
    eventsfile.open("/dev/input/event2", ios::in);
    if(eventsfile.is_open()) {
        cout << "File open for reading." << endl;
    }
    else {
        cout << "File couldn't be opened." << endl;
        return 0;
    }

    // Get the size of an input event in the right format!
    input_event ie;
    streamsize sie = static_cast<streamsize>(sizeof(struct input_event));

    while(eventsfile.read((char*)&ie, sie)) {

        // Read for non-zero event codes.
        if(ie.code != 0) {

            // Toggle the button state.
            map[ie.code].pressed = !map[ie.code].pressed;

            // On press
            if(map[ie.code].pressed) {
                gettimeofday(&map[ie.code].pressTime,NULL);
                cout << map[ie.code].name << " " << "DOWN" << endl;
            }

            // On release
            else {
                struct timeval ctime;
                gettimeofday(&ctime,NULL);

                // Calculate length of hold
                long usecs = ((ctime.tv_sec   -  map[ie.code].pressTime.tv_sec  )*1000000L
                              +ctime.tv_usec) -  map[ie.code].pressTime.tv_usec;

                // Print out press information
                cout << map[ie.code].name << " " << "UP (" << usecs << " microseconds)" << endl;

                // Check if MIDDLE was held for > 1 second
                if(map[ie.code].name == "Middle" && usecs > 1000000L) {
                    ifstream termfile;
                    // Then execute the contents of /etc/draft/.terminate
                    termfile.open("/opt/etc/draft/.terminate", ios::in);
                    if(termfile.is_open()) {
                        cout << "Termfile exists and can be read." << endl;
                        termfile.close();
                        system("/bin/bash /opt/etc/draft/.terminate");
                    }
                    else {
                        cout << "Termfile couldn't be read." << endl;
                    }

                }

            }
        }
    }

    eventsfile.close();

    return 0;
}
