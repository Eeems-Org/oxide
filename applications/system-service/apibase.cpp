#include "apibase.h"
#include "appsapi.h"


int APIBase::hasPermission(QString permission, const char* sender){
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    qDebug() << "Checking permission" << permission << "from" << sender;
    for(auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){
            auto result = app->permissions().contains(permission);
            qDebug() << app->name() << result;
            return result;
        }
    }
    qDebug() << "app not found, permission granted";
    return true;
}
