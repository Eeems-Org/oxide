#include "tarnish.h"
#include "meta.h"
#include "debug.h"

#include <QDBusConnection>

codes::eeems::oxide1::General* general_api = nullptr;
codes::eeems::oxide1::Power* power_api = nullptr;
codes::eeems::oxide1::Wifi* wifi_api = nullptr;
codes::eeems::oxide1::Screen* screen_api = nullptr;
codes::eeems::oxide1::Apps* apps_api = nullptr;
codes::eeems::oxide1::System* system_api = nullptr;
codes::eeems::oxide1::Notifications* notification_api = nullptr;

bool verifyConnection(){
    if(general_api == nullptr){
        return false;
    }
    if(!general_api->isValid()){
        delete general_api;
        general_api = nullptr;
        return false;
    }
    return true;
}

namespace Oxide::Tarnish {
    codes::eeems::oxide1::General* getAPI(){ return general_api; }
    QString requestAPI(std::string name){
        connect();
        auto reply = general_api->requestAPI(QString::fromStdString(name));
        if(reply.isError()){
            O_WARNING("Unable to request " << name.c_str() << " api:" << reply.error());
            return "/";
        }
        QDBusObjectPath path = reply;
        if(path.path() == "/"){
            O_WARNING("API " << name.c_str() << " request denied, or unavailable");
        }
        return path.path();
    }
    void connect(){
        if(verifyConnection()){
            return;
        }
        auto bus = QDBusConnection::systemBus();
        O_DEBUG("Waiting for tarnish to start up...");
        while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            timespec args{
                .tv_sec = 1,
                .tv_nsec = 0,
            }, res;
            nanosleep(&args, &res);
        }
        general_api = new codes::eeems::oxide1::General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, qApp);

    }
    void registerChild(){
        registerChild(qApp->applicationName().toStdString());
    }
    void registerChild(std::string name){
        connect();
        general_api->registerChild(getpid(), QString::fromStdString(name), QDBusUnixFileDescriptor(fileno(stdin)), QDBusUnixFileDescriptor(fileno(stdout)));
    }
    int tarnishPid(){
        connect();
        return general_api->tarnishPid();
    }
    codes::eeems::oxide1::Power* powerAPI(){
        if(power_api != nullptr){
            return power_api;
        }
        auto path = requestAPI("power");
        if(path == "/"){
            return nullptr;
        }
        power_api = new codes::eeems::oxide1::Power(OXIDE_SERVICE, path, general_api->connection(), (QObject*)qApp);
        return power_api;
    }

    codes::eeems::oxide1::Wifi* wifiAPI(){
        if(wifi_api != nullptr){
            return wifi_api;
        }
        auto path = requestAPI("wifi");
        if(path == "/"){
            return nullptr;
        }
        wifi_api = new codes::eeems::oxide1::Wifi(OXIDE_SERVICE, path, general_api->connection(), (QObject*)qApp);
        return wifi_api;
    }

    codes::eeems::oxide1::Screen* screenAPI(){
        if(screen_api != nullptr){
            return screen_api;
        }
        auto path = requestAPI("screen");
        if(path == "/"){
            return nullptr;
        }
        screen_api = new codes::eeems::oxide1::Screen(OXIDE_SERVICE, path, general_api->connection(), (QObject*)qApp);
        return screen_api;
    }

    codes::eeems::oxide1::Apps* appsAPI(){
        if(apps_api != nullptr){
            return apps_api;
        }
        auto path = requestAPI("apps");
        if(path == "/"){
            return nullptr;
        }
        apps_api = new codes::eeems::oxide1::Apps(OXIDE_SERVICE, path, general_api->connection(), (QObject*)qApp);
        return apps_api;
    }

    codes::eeems::oxide1::System* systemAPI(){
        if(system_api != nullptr){
            return system_api;
        }
        auto path = requestAPI("system");
        if(path == "/"){
            return nullptr;
        }
        system_api = new codes::eeems::oxide1::System(OXIDE_SERVICE, path, general_api->connection(), (QObject*)qApp);
        return system_api;
    }

    codes::eeems::oxide1::Notifications* notificationAPI(){
        if(notification_api != nullptr){
            return notification_api;
        }
        auto path = requestAPI("notification");
        if(path == "/"){
            return nullptr;
        }
        notification_api = new codes::eeems::oxide1::Notifications(OXIDE_SERVICE, path, general_api->connection(), (QObject*)qApp);
        return notification_api;
    }
}
