#include "tarnish.h"
#include "meta.h"
#include "debug.h"

#include <QDBusConnection>

General* api = nullptr;

bool verifyConnection(){
    if(api == nullptr){
        return false;
    }
    if(!api->isValid()){
        delete api;
        api = nullptr;
        return false;
    }
    return true;
}

namespace Oxide::Tarnish {
    General* getApi(){ return api; }
    QDBusObjectPath requestAPI(std::string name){
        connect();
        return api->requestAPI(QString::fromStdString(name));
    }
    void connect(){ connect(qApp->applicationName().toStdString()); }
    void connect(std::string name){
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
        api = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, qApp);
        api->registerChild(getpid(), QString::fromStdString(name), QDBusUnixFileDescriptor(fileno(stdin)), QDBusUnixFileDescriptor(fileno(stdout)));
    }
    int tarnishPid(){
        connect();
        return api->tarnishPid();
    }
}
