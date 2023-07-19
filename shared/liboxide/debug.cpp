#include "debug.h"
#include <fcntl.h>

namespace Oxide {
    bool debugEnabled(){
        if(getenv("DEBUG") == NULL){
            return false;
        }
        QString env = qgetenv("DEBUG");
        return !(QStringList() << "0" << "n" << "no" << "false").contains(env.toLower());
    }

    std::string getAppName(){
        if(!QCoreApplication::startingUp()){
            return qApp->applicationName().toStdString().c_str();
        }
        static std::string name;
        if(!name.empty()){
            return name.c_str();
        }
        QFile file("/proc/self/comm");
        if(file.open(QIODevice::ReadOnly)){
           name = file.readAll().toStdString();
        }
        if(!name.empty()){
            return name.c_str();
        }
        name = QFileInfo("/proc/self/exe").canonicalFilePath().toStdString();
        if(name.empty()){
            return "unknown";
        }
        return name.c_str();
    }

    std::string getDebugApplicationInfo(){
        return QString("[%1:%2:%3 %4]")
            .arg(::getpgrp())
            .arg(::getpid())
            .arg(::gettid())
            .arg(Oxide::getAppName().c_str())
            .toStdString();
    }

    std::string getDebugLocation(const char* file, unsigned int line, const char* function){
        return QString("(%1:%2, %3)")
            .arg(file)
            .arg(line)
            .arg(function)
            .toStdString();
    }

}
