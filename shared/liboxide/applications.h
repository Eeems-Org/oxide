#ifndef APPLICATIONS_H
#define APPLICATIONS_H

#include <QString>
#include <QFile>

#include <string>

namespace Oxide::Applications{
    enum ValidationError {
        None,
    };
    ValidationError validate(const char* path);
    ValidationError validate(const std::string& path);
    ValidationError validate(const QString& path);
    ValidationError validate(const FILE* file);
    ValidationError validate(const QFile* file);
}


#endif // APPLICATIONS_H
