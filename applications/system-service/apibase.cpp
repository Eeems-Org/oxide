#include "apibase.h"
#include "appsapi.h"

int APIBase::hasPermission(QString permission, const char* sender){
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    O_INFO("Checking permission" << permission << "from" << sender);
    for(auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){
            auto result = app->permissions().contains(permission);
            O_INFO(app->name() << result);
            return result;
        }
    }
    O_INFO("app not found, permission granted");
    return true;
}

int APIBase::getSenderPid() {
    if (!calledFromDBus()) {
        return getpid();
    }
    return connection().interface()->servicePid(message().service());
}
int APIBase::getSenderPgid() { return getpgid(getSenderPid()); }

#include "moc_apibase.cpp"
