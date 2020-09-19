#include <QDebug>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "appitem.h"
#include "dbusservice_interface.h"
#include "appsapi_interface.h"
#include "mxcfb.h"
#include "controller.h"

bool AppItem::ok(){
    if(_name.isEmpty()){
        qDebug() << "Missing name";
        return false;
    }
    if(_call.isEmpty()){
        qDebug() << "Missing executable";
        return false;
    }
    if(_displayName.isEmpty()){
        qDebug() << "Missing display name";
        return false;
    }
    if(!QFile(_call).exists()){
        qDebug() << "Executable doesn't exist";
        return false;
    }
    if(app != nullptr){
        return true;
    }
    auto bus = QDBusConnection::systemBus();
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    QDBusObjectPath path = api.requestAPI("apps");
    if(path.path() == "/"){
        qDebug() << "Unable to acces Apps API";
        return false;
    }
    Apps apps(OXIDE_SERVICE, path.path(), bus);
    QDBusObjectPath appPath;
    auto applications = apps.applications();
    if(!applications.contains(_name)){
        QVariantMap properties;
        properties.insert("name", _name);
        properties.insert("description", _desc);
        properties.insert("call", _call);
        properties.insert("icon", _imgFile);
        appPath = (QDBusObjectPath)apps.registerApplication(properties);
    }else{
        appPath = applications[_name].value<QDBusObjectPath>();
    }
    app = new Application(OXIDE_SERVICE, appPath.path(), bus, this);
    connect(app, &Application::exited, this, &AppItem::exited);
    if(!app->isValid()){
        delete app;
        app = nullptr;
        qDebug() << "Application API instance is invalid" << app->lastError();
        return false;
    }
    return true;
}

void AppItem::execute(){
    if(app == nullptr || !app->isValid()){
        qWarning() << "Application instance is not valid";
        return;
    }
    qDebug() << "Running application " << app->path();
    try {
        app->launch().waitForFinished();
    }catch(const std::exception& e){
        qDebug() << "Failed to launch" << property("name").toString() << e.what();
    }
    qDebug() << "Waiting for application to exit...";
}
void AppItem::stop(){
    if(app == nullptr || !app->isValid()){
        qWarning() << "Application instance is not valid";
        return;
    }
    app->stop();
}

void AppItem::exited(int exitCode){
    qDebug() << "Application exited" << exitCode;
}
