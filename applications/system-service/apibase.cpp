#include "apibase.h"
#include "appsapi.h"
#include "dbusservice.h"


bool APIBase::hasPermission(QString permission, const char* sender){
    if(DBusService::shuttingDown()){
        return false;
    }
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    O_DEBUG("Checking permission" << permission << "from" << sender);
    for(auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){
            auto result = app->permissions().contains(permission);
            O_DEBUG(app->name() << result);
            return result;
        }
    }
    O_DEBUG("app not found, permission granted");
    return true;
}

bool APIBase::hasPermissionStrict(QString permission, const char* sender){
    if(DBusService::shuttingDown()){
        return false;
    }
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    O_DEBUG("Checking permission" << permission << "from" << sender);
    for(auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){
            auto result = app->permissions().contains(permission);
            O_DEBUG(app->name() << result);
            return result;
        }
    }
    O_DEBUG("app not found, permission denied");
    return false;
}

#include "moc_apibase.cpp"

