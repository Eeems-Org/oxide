#include "sysobject.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>
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
        return std::stoi(strProperty(name));
    }
    std::string SysObject::strProperty(const std::string& name){
        auto path = propertyPath(name);
        QFile file(path.c_str());
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
            qDebug() << "Couldn't find the file " << path.c_str();
            return "";
        }
        QTextStream in(&file);
        std::string text = in.readLine().toStdString();
        // rtrim
        text.erase(std::find_if(text.rbegin(), text.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), text.end());
        return text;
    }
}

