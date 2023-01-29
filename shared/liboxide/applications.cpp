#include "applications.h"
#include "json.h"
#include "liboxide.h"

#include <QJsonObject>
#include <QFileInfo>
#include <QImage>

namespace Oxide::Applications{
    QTextStream& operator<<(QTextStream& s, const ErrorLevel& l){
        switch (l) {
            case ErrorLevel::Hint:
                s << "hint";
                break;
            case ErrorLevel::Deprecation:
                s << "deprecation";
                break;
            case ErrorLevel::Warning:
                s << "warning";
                break;
            case ErrorLevel::Error:
                s << "error";
                break;
            case ErrorLevel::Critical:
                s << "critical";
                break;
            default:
                s << "unknown";
        }
        return s;
    }
    QTextStream& operator<<(QTextStream& s, const ValidationError& e){
        s << e.level << ": " << e.msg;
        return s;
    }
    QList<ValidationError> validateRegistration(const char* path){ return validateRegistration(QString(path)); }

    QList<ValidationError> validateRegistration(const std::string& path){ return validateRegistration(QString(path.c_str())); }

    QList<ValidationError> validateRegistration(const QString& path){
        QFile file(path);
        if(!file.open(QFile::ReadOnly)){
            QList<ValidationError> errors;
            errors.append(ValidationError{
                .level = ErrorLevel::Critical,
                .msg = "Could not open file"
            });
            return errors;
        }
        auto res = validateRegistration(&file);
        file.close();
        return res;
    }

    QList<ValidationError> validateRegistration(QFile* file){
        QList<ValidationError> errors;
        auto data = file->readAll();
        auto app = QJsonDocument::fromJson(data).object();
        if(app.isEmpty()){
            errors.append(ValidationError{
                .level = ErrorLevel::Critical,
                .msg = "File is not valid JSON"
            });
            return errors;
        }
        auto contains = [app, &errors](const QString& name) -> bool{
            if(app.contains(name)){
                return true;
            }
            errors.append(ValidationError{
                .level = ErrorLevel::Critical,
                .msg = QString(
                    "Key \"%1\" is missing"
                ).arg(name)
            });
            return false;
        };
        auto isString = [app, &errors, contains](const QString& name, bool required) -> bool{
            if(required && !contains(name)){
                return false;
            }else if(!required && !app.contains(name)){
                return false;
            }
            if(app[name].isString()){
                return true;
            }
            errors.append(ValidationError{
                .level = required ? ErrorLevel::Error : ErrorLevel::Warning,
                .msg = QString(
                    "Key \"%1\" must contain a string"
                ).arg(name)
            });
            return false;
        };
        auto isFile = [app, &errors, isString, contains](const QString& name, const ErrorLevel& level, bool required) -> bool{
            if(!isString(name, required)){
                return false;
            }
            if(!app.contains(name)){
                return false;
            }
            auto value = app[name];
            auto str = value.toString();
            if(str.isEmpty()){
                return false;
            }
            if(QFile::exists(str)){
                return true;
            }
            errors.append(ValidationError{
                .level = level,
                .msg = QString(
                    "Value \"%1\" for key \"%2\" is a path that does not exist"
                ).arg(str, name)
            });
            return false;
        };
        auto isImage = [app, &errors, isFile, isString, contains](const QString& name, const ErrorLevel& level, bool required) -> bool{
            if(!isFile(name, level, required)){
                return false;
            }
            auto value = app[name].toString();
            QImage image;
            if(image.load(value) && !image.isNull()){
                return true;
            }
            errors.append(ValidationError{
                .level = level,
                .msg = QString(
                    "Value \"%1\" for key \"%2\" is a path to a file that is not a valid image"
                ).arg(value, name)
            });
        };
        //auto name = QFileInfo(file->fileName()).completeBaseName();
        QString type = app.contains("type") ? app["type"].toString().toLower() : "";
        if(type == "background"){
            // TODO validate any extra settings required by background apps
        }else if(type == "backgroundable"){
            // TODO validate any extra settings required by backgroundable apps
        }else if(type == "foreground" || type.isEmpty()){
            // TODO validate any extra settings required by foreground apps
        }else{
            errors.append(ValidationError{
                .level = ErrorLevel::Critical,
                .msg = QString(
                    "Value \"%1\" for key \"type\" is not valid"
                ).arg(type)
            });
        }
        if(isFile("bin", ErrorLevel::Error, true)){
            auto bin = app["bin"].toString();
            if(!QFileInfo(bin).isExecutable()){
                errors.append(ValidationError{
                    .level = ErrorLevel::Error,
                    .msg = QString(
                        "Value \"%1\" for key \"bin\" is a path to a file that is not executable"
                    ).arg(bin)
                });
            }
        }
        if(app.contains("directories")){
            if(!app["directories"].isArray()){
                errors.append(ValidationError{
                    .level = ErrorLevel::Critical,
                    .msg = "Key \"directories\" must contain an array"
                });
            }else{
                auto directories = app["directories"].toArray();
                for(int i = 0; i < directories.count(); i++){
                    QJsonValue entry = directories[i];
                    if(!entry.isString()){
                        errors.append(ValidationError{
                            .level = ErrorLevel::Error,
                            .msg = QString(
                                "Value \"%1\" for key \"directories[%2]\" contains an entry that is not a string \"%3\""
                            ).arg(Oxide::JSON::toJson(directories), QString::number(i), Oxide::JSON::toJson(entry))
                        });
                        continue;
                    }
                    auto directory = entry.toString();
                    if(!QFile::exists(directory)){
                        errors.append(ValidationError{
                            .level = ErrorLevel::Error,
                            .msg = QString(
                                "Value \"%1\" for key \"directories[%2]\" contains a path that does not exist \"%3\""
                            ).arg(Oxide::JSON::toJson(directories), QString::number(i), directory)
                        });
                    }
                }
            }
        }
        if(app.contains("permissions")){
            if(!app["permissions"].isArray()){
                errors.append(ValidationError{
                    .level = ErrorLevel::Critical,
                    .msg = "Key \"permissions\" must contain an array"
                });
            }else{
                auto permissions = app["permissions"].toArray();
                for(int i = 0; i < permissions.count(); i++){
                    QJsonValue entry = permissions[i];
                    if(!entry.isString()){
                        errors.append(ValidationError{
                            .level = ErrorLevel::Error,
                            .msg = QString(
                                "Value \"%1\" for key \"permissions[%2]\" contains a value that is not a string \"%3\""
                            ).arg(Oxide::JSON::toJson(permissions), QString::number(i), Oxide::JSON::toJson(entry))
                        });
                    }
                }
            }
        }
        isFile("workingDirectory", ErrorLevel::Error, false);
        isString("displayName", false);
        isString("description", false);
        if(isString("user", false)){
            auto user = app["user"].toString();
            try{
                Oxide::getUID(user);
            }catch(const std::exception& e){
                errors.append(ValidationError{
                    .level = ErrorLevel::Error,
                    .msg = QString(
                        "Value \"%1\" for key \"user\" is not a valid user: \"%2\""
                    ).arg(user, e.what())
                });
            }
        }
        if(isString("group", false)){
            auto group = app["group"].toString();
            try{
                Oxide::getGID(group);
            }catch(const std::exception& e){
                errors.append(ValidationError{
                    .level = ErrorLevel::Error,
                    .msg = QString(
                        "Value \"%1\" for key \"group\" is not a valid group: \"%2\""
                    ).arg(group, e.what())
                });
            }
        }
        isImage("icon", ErrorLevel::Warning, false);
        isImage("splash", ErrorLevel::Warning, false);
        return errors;
    }
}
