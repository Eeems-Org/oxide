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

bool AppItem::ok(){ return getApp() != nullptr; }

void AppItem::execute(){
    if(!getApp() || !app->isValid()){
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
    if(!getApp() || !app->isValid()){
        qWarning() << "Application instance is not valid";
        return;
    }
    app->stop();
}

void AppItem::exited(int exitCode){
    qDebug() << "Application exited" << exitCode;
}

Application* AppItem::getApp(){
    if(app != nullptr){
        return app;
    }
    auto controller = reinterpret_cast<Controller*>(parent());
    auto apps = controller->getAppsApi();
    auto bus = QDBusConnection::systemBus();
    QDBusObjectPath appPath;
    auto applications = apps->applications();
    if(!applications.contains(_name)){
        qDebug() << "Couldn't find Application instance";
        return nullptr;
    }
    appPath = applications[_name].value<QDBusObjectPath>();
    auto instance = new Application(OXIDE_SERVICE, appPath.path(), bus, this);
    if(!instance->isValid()){
        delete instance;
        qDebug() << "Application API instance is invalid" << app->lastError();
        return nullptr;
    }
    connect(instance, &Application::exited, this, &AppItem::exited);
    app = instance;
    return app;
}
