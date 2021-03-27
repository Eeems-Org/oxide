#include "fifohandler.h"
#include "systemapi.h"

void FifoHandler::handleLine(const QString& line){
    if(name == "powerState"){
        if(!hasPermission("power")){
            qWarning() << "Denied powerState request for " << name;
            return;
        }
        if((QStringList() << "mem" << "freeze" << "standby").contains(line)){
            systemAPI->suspend();
        }else{
            qWarning() << "Unknown power state call: " << line;
        }
    }
}
