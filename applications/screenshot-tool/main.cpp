#include <QCoreApplication>
#include <QDebug>
#include <QUuid>

#include <cstdlib>
#include <signal.h>
#include <liboxide.h>
#include <liboxide/tarnish.h>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;

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
    QCoreApplication app(argc, argv);
    sentry_init("fret", argv);
    atexit(onExit);
    signal(SIGTERM, unixSignalHandler);
    signal(SIGSEGV, unixSignalHandler);
    signal(SIGABRT, unixSignalHandler);
    signal(SIGSYS, unixSignalHandler);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("fret");
    app.setApplicationVersion(APP_VERSION);
    Oxide::Tarnish::getSocketFd();
    auto system = Oxide::Tarnish::systemAPI();
    if(system == nullptr){
        qDebug() << "Unable to get system API";
        return EXIT_FAILURE;
    }
    qDebug() << "Requesting screen API...";
    auto screen = Oxide::Tarnish::screenAPI();
    if(screen == nullptr){
        qDebug() << "Unable to get screen API";
        return EXIT_FAILURE;
    }
    qDebug() << "Requesting notification API...";
    auto notifications = Oxide::Tarnish::notificationAPI();
    if(notifications == nullptr){
        qDebug() << "Unable to get notification API";
        return EXIT_FAILURE;
    }
    qDebug() << "Connecting signal listener...";
    auto bus = notifications->connection();
    QObject::connect(system, &System::rightAction, [screen, notifications, bus, &app]{
        qDebug() << "Taking screenshot";
        auto reply = screen->screenshot();
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << "Failed to take screenshot";
            addNotification(notifications, "Screenshot failed: " + reply.error().message());
            return;
        }
        auto qPath = reply.value().path();
        if(qPath == "/"){
            qDebug() << "Failed to take screenshot";
            addNotification(notifications, "Screenshot failed: Unknown reason");
            return;
        }
        Screenshot screenshot(OXIDE_SERVICE, qPath, bus, &app);
        if(QFile("/tmp/.screenshot").exists()){
            // Then execute the contents of /tmp/.screenshot
            qDebug() << "Screenshot file exists.";
            QProcess::execute("/bin/bash", QStringList() << "/tmp/.screenshot" << screenshot.path());
        }
        addNotification(notifications, "Screenshot taken", screenshot.path());
        qDebug() << "Screenshot done.";
    });
    qDebug() << "Waiting for signals...";
    return app.exec();
}
