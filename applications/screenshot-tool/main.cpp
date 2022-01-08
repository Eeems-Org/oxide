#include <QCoreApplication>
#include <QDebug>
#include <QUuid>

#include <cstdlib>
#include <signal.h>
#include <liboxide.h>

#include "dbussettings.h"
#include "devicesettings.h"

#include "dbusservice_interface.h"
#include "systemapi_interface.h"
#include "screenapi_interface.h"
#include "screenshot_interface.h"
#include "notificationapi_interface.h"
#include "notification_interface.h"

using namespace codes::eeems::oxide1;

void unixSignalHandler(int signal){
    qDebug() << "Recieved signal" << signal;
    exit(EXIT_SUCCESS);
}
void onExit(){
    qDebug() << "Exiting";
}

void addNotification(Notifications* notifications, QString text, QString icon = ""){
    auto guid = QUuid::createUuid().toString();
    qDebug() << "Adding notification" << guid;
    QDBusObjectPath path = notifications->add(guid, "codes.eeems.fret", text, icon);
    if(path.path() != "/"){
        auto notification = new Notification(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), notifications->parent());
        qDebug() << "Displaying notification" << guid;
        notification->display().waitForFinished();
        QObject::connect(notification, &Notification::clicked, [notification]{
            qDebug() << "Notification clicked" << notification->identifier();
            notification->remove();
            notification->deleteLater();
        });
    }else{
        qDebug() << "Failed to add notification";
    }
}

int main(int argc, char *argv[]){
    initSentry("fret", argv);
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
    qDebug() << "Requesting notification API...";
    path = api.requestAPI("notification");
    if(path.path() == "/"){
        qDebug() << "Unable to get notification API";
        return EXIT_FAILURE;
    }
    Notifications notifications(OXIDE_SERVICE, path.path(), bus, &app);
    qDebug()  << "Connecting signal listener...";
    QObject::connect(&system, &System::rightAction, [&screen, &notifications, bus, &app]{
        qDebug() << "Taking screenshot";
        auto reply = screen.screenshot();
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << "Failed to take screenshot";
            addNotification(&notifications, "Screenshot failed: " + reply.error().message());
            return;
        }
        auto qPath = reply.value().path();
        if(qPath == "/"){
            qDebug() << "Failed to take screenshot";
            addNotification(&notifications, "Screenshot failed: Unknown reason");
            return;
        }
        Screenshot screenshot(OXIDE_SERVICE, qPath, bus, &app);
        if(QFile("/tmp/.screenshot").exists()){
            // Then execute the contents of /tmp/.screenshot
            qDebug() << "Screenshot file exists.";
            QProcess::execute("/bin/bash", QStringList() << "/tmp/.screenshot" << screenshot.path());
        }
        addNotification(&notifications, "Screenshot taken", screenshot.path());
        qDebug() << "Screenshot done.";
    });
    qDebug() << "Waiting for signals...";
    return app.exec();
}
