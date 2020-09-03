#include "taskitem.h"
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <memory>
#include <signal.h>

using namespace std;

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
    result.erase(std::find_if(result.rbegin(), result.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), result.end());
    return result;
}

TaskItem::TaskItem(std::string pid) : QObject(nullptr), _pid(stoi(pid)){
    std::string path = "/proc/" + to_string(_pid) + "/status";
    _name = QString::fromStdString(exec(("cat " + path + " | grep Name: | awk '{print$2}'").c_str())).trimmed();
    _ppid = stoi(exec(("cat " + path + " | grep PPid: | awk '{print$2}'").c_str()));
    _killable = _ppid;
}

bool TaskItem::signal(int signal){
    return kill(_pid, signal);
}
