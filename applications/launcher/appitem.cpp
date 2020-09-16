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
    if(_name.isEmpty() || _call.isEmpty()){
        return false;
    }
    if(app == nullptr){
        auto bus = QDBusConnection::systemBus();
        General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        QDBusObjectPath path = api.requestAPI("apps");
        if(path.path() != "/"){
            Apps apps(OXIDE_SERVICE, path.path(), bus);
            QDBusObjectPath appPath;
            auto applications = apps.applications();
            if(!applications.contains(_name)){
                QVariantMap properties;
                properties.insert("name", _name);
                properties.insert("description", _desc);
                properties.insert("call", _call);
                properties.insert("term", _term);
                appPath = (QDBusObjectPath)apps.registerApplication(properties);
            }else{
                appPath = applications[_name].value<QDBusObjectPath>();
            }
            app = new Application(OXIDE_SERVICE, appPath.path(), bus, this);
            connect(app, &Application::exited, this, &AppItem::exited);
            return app->isValid();
        }
    }
    return true;
}

void AppItem::execute(){
    qDebug() << "Setting termfile /tmp/.terminate";
    std::ofstream termfile;
    termfile.open("/tmp/.terminate");
    termfile << _term.toStdString() << std::endl;
    qDebug() << "Running application " << app->path();
    try {
        app->launch().waitForFinished();
    }catch(const std::exception& e){
        qDebug() << "Failed to launch" << property("name").toString() << e.what();
    }
    qDebug() << "Waiting for application to exit...";
}

void AppItem::exited(int exitCode){
    qDebug() << "Application exited" << exitCode;
}
