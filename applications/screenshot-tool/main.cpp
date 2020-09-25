#include <QCoreApplication>
#include <QDebug>

#include <cstdlib>
#include <signal.h>

#include "dbussettings.h"

#include "dbusservice_interface.h"
#include "systemapi_interface.h"
#include "screenapi_interface.h"

using namespace codes::eeems::oxide1;

void unixSignalHandler(int signal){
    qDebug() << "Recieved signal" << signal;
    exit(EXIT_SUCCESS);
}
void onExit(){
    qDebug() << "Exiting";
}

int main(int argc, char *argv[]){
    atexit(onExit);
    signal(SIGTERM, unixSignalHandler);
    signal(SIGSEGV, unixSignalHandler);
    signal(SIGABRT, unixSignalHandler);
    signal(SIGSYS, unixSignalHandler);
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("fret");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    auto bus = QDBusConnection::systemBus();
    qDebug() << "Waiting for tarnish to start up...";
    while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
        struct timespec args{
            .tv_sec = 1,
            .tv_nsec = 0,
        }, res;
        nanosleep(&args, &res);
    }
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, &app);
    qDebug() << "Requesting system API...";
    QDBusObjectPath path = api.requestAPI("system");
    if(path.path() == "/"){
        qDebug() << "Unable to get system API";
        return EXIT_FAILURE;
    }
    System system(OXIDE_SERVICE, path.path(), bus, &app);
    qDebug() << "Requesting screen API...";
    path = api.requestAPI("screen");
    if(path.path() == "/"){
        qDebug() << "Unable to get screen API";
        return EXIT_FAILURE;
    }
    Screen screen(OXIDE_SERVICE, path.path(), bus, &app);
    qDebug()  << "Connecting signal listener...";
    QObject::connect(&system, &System::rightAction, [&screen]{
        qDebug() << "Taking screenshot";
        int res = screen.screenshot();
        if(res){
            qDebug() << "Failed to take screenshot: " << res;
        }
        if(QFile("/tmp/.screenshot").exists()){
            // Then execute the contents of /tmp/.screenshot
            qDebug() << "Screenshot file exists.";
            ::system("/bin/bash /tmp/.screenshot");
        }
        qDebug() << "Screenshot done.";
    });
    qDebug() << "Waiting for signals...";
    return app.exec();
}
