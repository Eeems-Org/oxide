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
