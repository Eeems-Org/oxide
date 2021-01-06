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

TaskItem::TaskItem(int pid) : QObject(nullptr), _pid(pid){
    QString file_content=readFile(QString::fromStdString("/proc/" + to_string(_pid) + "/status"));
    _name = parseRegex(file_content,QRegularExpression("^Name:\\t+(\\w+)"));
    _ppid = parseRegex(file_content,QRegularExpression("^PPid:\\t+(\\d+)",QRegularExpression::MultilineOption)).toInt();
    _killable = _ppid;
}

bool TaskItem::signal(int signal){
    return kill(_pid, signal);
}

QString TaskItem::parseRegex(QString &file_content, const QRegularExpression &reg){
    QRegularExpressionMatchIterator i = reg.globalMatch(file_content);
    if (!i.isValid()){
         return "";
    }
    QString result="";
    while (i.hasNext()){
          QRegularExpressionMatch match = i.next();
          result=match.captured(1);
    }
    return result;
}

QString TaskItem::readFile(const QString &path){
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){
        qDebug()<<"Error reading file: " + path;
        return "";
    }
    auto data = file.readAll();
    file.close();
    return data;
}
