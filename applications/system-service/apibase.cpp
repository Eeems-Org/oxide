#include "apibase.h"
#include "appsapi.h"

int APIBase::hasPermission(QString permission){
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    for(auto name : appsAPI->runningApplications().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){

        }
    }
    return true;
}
