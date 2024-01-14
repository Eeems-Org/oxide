/*!
 * \file
 */
//! [debugEnabled]
if(Oxide::debugEnabled()){
     qDebug() << "Debugging is enabled";
}
//! [debugEnabled]
//! [dispatchToMainThread]
Oxide::dispatchToMainThread([=]{
     qDebug() << "This is running on the main thread";
});
//! [dispatchToMainThread]
//! [SignalHandler]
SignalHandler::setup_unix_signal_handlers();

connect(signalHandler, &SignalHandler::sigUsr1, [=]{
     qDebug() << "SIGUSR1 recieved";
});
//! [SignalHandler]
//! [getGID]
try{
     auto gid = Oxide::getGID("admin");
     qDebug() << "admin GID is " << gid;
}catch(const std::exception& e){
     qDebug() << "Failed to get group: " << e.what();
}
//! [getGID]
//! [getUID]
try{
     auto gid = Oxide::getUID("root");
     qDebug() << "root UID is " << gid;
}catch(const std::exception& e){
     qDebug() << "Failed to get user: " << e.what();
}
//! [getUID]
//! [EventFilter]
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include <QtPlugin>

#import <liboxide>
#include <liboxide/eventfilter.h>

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

using namespace Oxide;
int main(int argc, char *argv[]){
     QGuiApplication app(argc, argv);
     QQmlApplicationEngine engine;
     QQmlContext* context = engine.rootContext();
     context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
     engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
     if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
     }

     auto root = engine.rootObjects().first();
     root->installEventFilter(new EventFilter(&app));
     return app.exec();
}
//! [EventFilter]
//! [SysObject]
SysObject lo("/sys/class/net/lo");
if(lo.exists()){
     qDebug() << "Loop back mac address: " << lo.strProperty("address").c_str();
     qDebug() << "Does " << lo.propertyPath("power").c_str() << " exist?" << lo.hasDirectory("power");
}else{
     qCritical("Loopback is missing?");
}
//! [SysObject]
//! [setupQtEnvironment]
#include <liboxide.h>
#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

int main(int argc, char* argv[]){
     deviceSettings.setupQtEnvironment();
}
//! [setupQtEnvironment]
