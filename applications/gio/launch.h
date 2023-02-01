#pragma once

#include "common.h"

#include <liboxide.h>
#include <liboxide/applications.h>

#include <QProcess>
#include <QUrl>
#include <QFileInfo>
#include <QUuid>

using namespace codes::eeems::oxide1;
using namespace Oxide::Applications;

class LaunchCommand : ICommand{
    O_COMMAND(LaunchCommand, "launch", "Launch an application from an application registration file")
    int arguments() override{
        parser->addPositionalArgument("DESKTOP-FILE", "Application registration to launch");
        parser->addPositionalArgument("FILE-ARG", "NOT IMPLEMENTED", "[FILE-ARG...]");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        const auto& path = args.first();
        auto url = urlFromPath(path);
        if(!url.isLocalFile()){
            GIO_ERROR(url, path, "Error while parsing path", "Path is not a local file");
            return EXIT_FAILURE;
        }
        auto suffix = QFileInfo(url.path()).suffix();
        if(suffix != "oxide"){
            GIO_ERROR(url, path, "Unhandled file extension", suffix);
            return EXIT_FAILURE;
        }
        auto reg = getRegistration(path);
        if(reg.isEmpty()){
            GIO_ERROR(url, path, "Invalid application registration", "Invalid JSON");
            return EXIT_FAILURE;
        }
        if(!reg.contains("flags")){
            reg["flags"] = QJsonArray();
        }
        if(!reg["flags"].isArray()){
            GIO_ERROR(url, path, "Invalid application registration", "Key \"flags\" must be an array");
            return EXIT_FAILURE;
        }
        auto flags = reg["flags"].toArray();
        flags.prepend("transient");
        reg["flags"] = flags;
        if(!reg.contains("displayName")){
            QFileInfo info(path);
            // Workaround because basename will sometimes return the suffix instead
            reg["displayName"] = info.fileName().left(info.fileName().length() - 6);
        }
        auto name = QUuid::createUuid().toString();
        auto errors = validateRegistration(name, reg);
        if(std::any_of(errors.constBegin(), errors.constEnd(), [](const ValidationError& error){
            auto level = error.level;
            return level == ErrorLevel::Error || level == ErrorLevel::Critical;
        })){
            for(const auto& error : errors){
                if(error.level == ErrorLevel::Error || error.level == ErrorLevel::Critical){
                    GIO_ERROR(url, path, "Invalid application registration", error)
                }
            }
            return EXIT_FAILURE;
        }
        auto bus = QDBusConnection::systemBus();
        General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        QDBusObjectPath qpath = api.requestAPI("apps");
        if(qpath.path() == "/"){
            GIO_ERROR(url, path, "Error registering transient application", "Unable to get apps API");
            return EXIT_FAILURE;
        }
        Apps apps(OXIDE_SERVICE, qpath.path(), bus);
        auto properties = registrationToMap(reg, name);
        if(properties.isEmpty()){
            GIO_ERROR(url, path, "Error registering transient application", "Unable to convert application registration to QVariantMap");
            return EXIT_FAILURE;
        }
        qpath = apps.registerApplication(properties);
        if(qpath.path() == "/"){
            GIO_ERROR(url, path, "Error registering transient application", "Failed to register" << name.toStdString().c_str());
            return EXIT_FAILURE;
        }
        Application app(OXIDE_SERVICE, qpath.path(), bus);
        app.launch().waitForFinished();
        return EXIT_SUCCESS;
    }
};
