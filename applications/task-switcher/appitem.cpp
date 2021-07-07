#include <QDebug>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "appitem.h"
#include "dbusservice_interface.h"
#include "appsapi_interface.h"
#include "controller.h"

bool AppItem::ok(){ return getApp() != nullptr; }

void AppItem::execute(){
    if(!getApp() || !app->isValid()){
        qWarning() << "Application instance is not valid";
        return;
    }
    qDebug() << "Running application " << property("name").toString();
    auto reply = app->launch();
    while(!reply.isFinished()){
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
    }
    if(reply.isError()){
        qDebug() << "Failed to launch" << property("name").toString() << reply.error().message();
    }
}
void AppItem::stop(){
    if(!getApp() || !app->isValid()){
        qWarning() << "Application instance is not valid";
        return;
    }
    app->stop();
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
    connect(instance, &Application::displayNameChanged, this, &AppItem::onDisplayNameChanged);
    connect(instance, &Application::iconChanged, this, &AppItem::onIconChanged);
    connect(instance, &Application::launched, this, &AppItem::launched);
    app = instance;
    return app;
}
