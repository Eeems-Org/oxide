#include "sysobject.h"
#include "debug.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <sstream>
#include <fstream>

namespace Oxide{
    std::string SysObject::propertyPath(const std::string& name){
        return m_path + "/" + name;
    }

    bool SysObject::exists(){
        QDir dir(m_path.c_str());
        return dir.exists();
    }
    bool SysObject::hasProperty(const std::string& name){
        QFile file(propertyPath(name).c_str());
        return file.exists();
    }
    bool SysObject::hasDirectory(const std::string& name){
        QDir dir(propertyPath(name).c_str());
        return dir.exists();
    }
    int SysObject::intProperty(const std::string& name){
        if(!hasProperty(name)){
            return 0;
        }
        try {
            return std::stoi(strProperty(name));
        }
        catch (const std::invalid_argument& e) {
            O_DEBUG("Property value is not an integer: " << name.c_str());
            return 0;
        }
    }
    std::string SysObject::strProperty(const std::string& name){
        auto path = propertyPath(name);
        QFile file(path.c_str());
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
            O_DEBUG("Couldn't find the file:" << path.c_str());
            return "0";
        }
        QTextStream in(&file);
        std::string text = in.readLine().toStdString();
        // rtrim
        text.erase(std::find_if(text.rbegin(), text.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), text.end());
        return text;
    }
    QMap<QString, QString> SysObject::uevent(){
        auto path = propertyPath("uevent");
        QFile file(path.c_str());
        QMap<QString, QString> props;
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
            O_DEBUG("Couldn't find the file:" << path.c_str());
            return props;
        }
        QTextStream in(&file);
        do{
            auto line = in.readLine();
            // doesn't always properly provide EOF
            if(line.trimmed().isEmpty()){
                break;
            }
            auto parts = line.split("=");
            if(parts.length() != 2){
                O_WARNING("Invalid uevent line" << line);
                continue;
            }
            props.insert(parts.first().trimmed(), parts.last().trimmed());
        }while(!in.atEnd());
        return props;
    }
}

