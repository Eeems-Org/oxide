#include "debug.h"

namespace Oxide {
    bool debugEnabled(){
        if(getenv("DEBUG") == NULL){
            return false;
        }
        QString env = qgetenv("DEBUG");
        return !(QStringList() << "0" << "n" << "no" << "false").contains(env.toLower());
    }
}
