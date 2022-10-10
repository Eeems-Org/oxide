#ifndef SYSOBJECT_H
#define SYSOBJECT_H

#include <string>
#include <QString>

class SysObject
{
public:
    explicit SysObject(QString path) : m_path(path.toStdString()){};
    std::string path() { return m_path; }
    bool exists();
    bool hasProperty(const std::string& name);
    bool hasDirectory(const std::string& name);
    std::string strProperty(const std::string& name);
    int intProperty(const std::string& name);
    std::string propertyPath(const std::string& name);

private:
    std::string m_path;
};

#endif // SYSOBJECT_H
