#include "taskitem.h"
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>
#include <stdio.h>

TaskItem::TaskItem(std::string pid) : QObject(nullptr), _pid(pid.c_str()){
    std::string path = "/proc/" + _pid.toStdString() + "/status";
    FILE* file = fopen(path.c_str(), "r");
    if(!file){
        return;
    }
    std::ifstream stream(path.c_str());
    // Get the second line
    std::string line;
    std::getline(stream, line);
    line = line.substr(6, line.length() - 6);
    _name = QString(line.c_str());
}
bool TaskItem::ok(){
    return _name.length();
}
