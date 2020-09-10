#ifndef SYSOBJECT_H
#define SYSOBJECT_H

#include <string>

class SysObject
{
public:
    explicit SysObject(std::string path) : m_path(path){};
    bool exists();
    bool hasProperty(std::string name);
    std::string strProperty(std::string name);
    int intProperty(std::string name);
    std::string propertyPath(std::string name);

private:
    std::string m_path;
};

#endif // SYSOBJECT_H
