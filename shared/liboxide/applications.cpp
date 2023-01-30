#include "applications.h"
#include "json.h"
#include "liboxide.h"

#include <QJsonObject>
#include <QFileInfo>
#include <QImage>
#include <QVariantMap>

using namespace codes::eeems::oxide1;

const QList<QString> SystemFlags {
    "system",
};
const QList<QString> Flags {
    "autoStart",
    "chroot",
    "hidden",
    "nosplash",
    "nosavescreen",
    "system",
};

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
    QVariantMap registrationToMap(const QJsonObject& app, const QString& name){
        auto _name = name;
        if(name.isEmpty()){
            if(!app.contains("name") || !app["name"].isString() || app["name"].toString().isEmpty()){
                return QVariantMap();
            }
            _name = app["name"].toString();
        }
        int type = Foreground;
        QString typeString = app.contains("type") ? app["type"].toString().toLower() : "";
        if(typeString == "background"){
            type = Background;
        }else if(typeString == "backgroundable"){
            type = Backgroundable;
        }
        // Anything else defaults to Foreground
        auto flags = QStringList();
        if(app.contains("flags")){
            for(auto flag : app["flags"].toArray()){
                if(!flag.isString()){
                    continue;
                }
                auto value = flag.toString();
                if(!value.isEmpty()){
                    flags << value;
                }
            }
        }
        QVariantMap properties {
            {"name", _name},
            {"bin", app["bin"].toString()},
            {"type", type},
            {"flags", flags},
        };
        if(app.contains("displayName")){
            properties.insert("displayName", app["displayName"].toString());
        }
        if(app.contains("description")){
            properties.insert("description", app["description"].toString());
        }
        if(app.contains("icon")){
            properties.insert("icon", app["icon"].toString());
        }
        if(app.contains("user")){
            properties.insert("user", app["user"].toString());
        }
        if(app.contains("group")){
            properties.insert("group", app["group"].toString());
        }
        if(app.contains("workingDirectory")){
            properties.insert("workingDirectory", app["workingDirectory"].toString());
        }
        if(app.contains("directories")){
            QStringList directories;
            for(auto directory : app["directories"].toArray()){
                directories.append(directory.toString());
            }
            properties.insert("directories", directories);
        }
        if(app.contains("permissions")){
            QStringList permissions;
            for(auto permission : app["permissions"].toArray()){
                permissions.append(permission.toString());
            }
            properties.insert("permissions", permissions);
        }
        if(app.contains("events")){
            auto events = app["events"].toObject();
            for(auto event : events.keys()){
                if(event == "stop"){
                    properties.insert("onStop", events[event].toString());
                }else if(event == "pause"){
                    properties.insert("onPause", events[event].toString());
                }else if(event == "resume"){
                    properties.insert("onResume", events[event].toString());
                }
            }
        }
        if(app.contains("environment")){
            QVariantMap envMap;
            auto environment = app["environment"].toObject();
            for(auto key : environment.keys()){
                envMap.insert(key, environment[key].toString());
            }
            properties.insert("environment", envMap);
        }
        if(app.contains("splash")){
            properties.insert("splash", app["splash"].toString());
        }
        return properties;
    }
    QList<ValidationError> validateRegistration(const char* path){ return validateRegistration(QString(path)); }
    QList<ValidationError> validateRegistration(const std::string& path){ return validateRegistration(QString(path.c_str())); }
    QList<ValidationError> validateRegistration(const QString& path){
        QFile file(path);
        auto res = validateRegistration(&file);
        if(file.isOpen()){
            file.close();
        }
        return res;
    }
    QList<ValidationError> validateRegistration(QFile* file){
        if(!file->isOpen() && !file->open(QFile::ReadOnly)){
            QList<ValidationError> errors;
            errors.append(ValidationError{
                .level = ErrorLevel::Critical,
                .msg = "Could not open file"
            });
            return errors;
        }
        auto data = file->readAll();
        auto app = QJsonDocument::fromJson(data).object();
        if(!app.isEmpty()){
            return validateRegistration(QFileInfo(file->fileName()).completeBaseName(), app);
        }
        QList<ValidationError> errors;
        errors.append(ValidationError{
            .level = ErrorLevel::Critical,
            .msg = "File is not valid JSON"
        });
        return errors;
    }
    QList<ValidationError> validateRegistration(const QString& name, const QJsonObject& app){
        QList<ValidationError> errors;
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
        auto isArray = [app, &errors, contains](const QString& name, const ErrorLevel& level, bool required) -> bool{
            if(required && !contains(name)){
                return false;
            }else if(!required && !app.contains(name)){
                return false;
            }
            if(app[name].isArray()){
                return true;
            }
            errors.append(ValidationError{
                .level = level,
                .msg = QString(
                    "Key \"%1\" must contain an array"
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
            return false;
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
                .level = ErrorLevel::Warning,
                .msg = QString(
                    "Value \"%1\" for key \"type\" is not valid and will default to foreground"
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
        QStringList flags;
        if(isArray("flags", ErrorLevel::Critical, false)){
            auto flagsArray = app["flags"].toArray();
            auto flagsJson = Oxide::JSON::toJson(flagsArray);
            for(auto flag : flagsArray){
                if(!flag.isString()){
                    errors.append(ValidationError{
                        .level = ErrorLevel::Error,
                        .msg = QString(
                            "Value \"%1\" for key \"flags\" contains an entry that is not a string \"%2\""
                        ).arg(flagsJson, Oxide::JSON::toJson(flag.toVariant()))
                    });
                    continue;
                }
                auto value = flag.toString().trimmed();
                if(value.isEmpty()){
                    errors.append(ValidationError{
                        .level = ErrorLevel::Error,
                        .msg = QString(
                            "Value \"%1\" for key \"flags\" contains an entry that is empty"
                        ).arg(flagsJson)
                    });
                    continue;
                }
                if(SystemFlags.contains(value)){
                    errors.append(ValidationError{
                        .level = ErrorLevel::Warning,
                        .msg = QString(
                            "Value \"%1\" for key \"flags\" contains an entry that should only be used by the system \"%2\""
                        ).arg(flagsJson, value)
                    });
                }else if(!Flags.contains(value)){
                    errors.append(ValidationError{
                        .level = ErrorLevel::Warning,
                        .msg = QString(
                            "Value \"%1\" for key \"flags\" contains an entry that is not known \"%2\""
                        ).arg(flagsJson, value)
                    });
                }
                flags << value;
            }
            if(type == "background" && flags.contains("nosavescreen")){
                errors.append(ValidationError{
                    .level = ErrorLevel::Hint,
                    .msg = "Key \"flags\" contains \"nosavescreen\" while \"type\" has value \"background\""
                });
            }
        }
        if(isArray("directories", ErrorLevel::Critical, false)){
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
        }else if(flags.contains("chroot")){
            errors.append(ValidationError{
                .level = ErrorLevel::Hint,
                .msg = "Key \"flags\" contains \"chroot\" while \"directories\" is missing"
            });
        }
        if(isArray("permissions", ErrorLevel::Critical, false)){
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
        if(isImage("splash", ErrorLevel::Warning, false) && flags.contains("nosplash")){
            errors.append(ValidationError{
                .level = ErrorLevel::Hint,
                .msg = "Key \"splash\" provided while \"flags\" contains \"nosplash\" value"
            });
        }
        if(registrationToMap(app, name).isEmpty()){
            errors.append(ValidationError{
                .level = ErrorLevel::Critical,
                .msg = "Unable to convert registration to QVariantMap"
            });
        }
        return errors;
    }
    bool addToTarnishCache(const char* path){ return addToTarnishCache(QString(path)); }
    bool addToTarnishCache(const std::string& path){ return addToTarnishCache(QString(path.c_str())); }
    bool addToTarnishCache(const QString& path){
        QFile file(path);
        auto res = addToTarnishCache(&file);
        if(file.isOpen()){
            file.close();
        }
        return res;
    }
    bool addToTarnishCache(QFile* file){
        if(!file->isOpen() && !file->open(QFile::ReadOnly)){
            return false;
        }
        auto data = file->readAll();
        auto app = QJsonDocument::fromJson(data).object();
        if(app.isEmpty()){
            return false;
        }
        auto name = QFileInfo(file->fileName()).completeBaseName();
        auto errors = validateRegistration(name, app);
        for(auto error : errors){
            if(error.level != ErrorLevel::Critical && error.level != ErrorLevel::Error){
                continue;
            }
            return false;
        }
        auto bus = QDBusConnection::systemBus();
        General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        QDBusObjectPath path = api.requestAPI("apps");
        if(path.path() == "/"){
            return false;
        }
        Apps apps(OXIDE_SERVICE, path.path(), bus);
        auto properties = registrationToMap(app, name);
        if(properties.isEmpty()){
            return false;
        }
        path = apps.registerApplication(properties);
        return path.path() != "/";
    }
}
