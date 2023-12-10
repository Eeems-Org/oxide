#include "debug.h"
#include <fcntl.h>
#include <execinfo.h>

namespace Oxide {
    bool debugEnabled(){
        if(getenv("DEBUG") == NULL){
            return false;
        }
        QString env = qgetenv("DEBUG");
        return !(QStringList() << "0" << "n" << "no" << "false").contains(env.toLower());
    }

    std::string getAppName(bool ignoreQApp){
        if(!ignoreQApp && !QCoreApplication::startingUp()){
            return qApp->applicationName().toStdString().c_str();
        }
        static std::string name;
        if(!name.empty()){
            return name.c_str();
        }
        QFile file("/proc/self/comm");
        if(file.open(QIODevice::ReadOnly)){
           name = file.readAll().trimmed().toStdString();
        }
        if(!name.empty()){
            return name.c_str();
        }
        name = QFileInfo("/proc/self/exe").canonicalFilePath().trimmed().toStdString();
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
            .arg(Oxide::getAppName(false).c_str())
            .toStdString();
    }

    std::string getDebugLocation(const char* file, unsigned int line, const char* function){
        return QString("(%1:%2, %3)")
            .arg(file)
            .arg(line)
            .arg(function)
            .toStdString();
    }

    std::vector<std::string> backtrace(unsigned short depth){
        void* array[depth];
        size_t size = ::backtrace(array, depth);
        char** messages = ::backtrace_symbols(array, size);
        std::vector<std::string> stack;
        stack.reserve(size);
        for(size_t i = 1; i < size && messages != NULL; ++i){
            stack.push_back(messages[i]);
        }
        return stack;
    }

}
