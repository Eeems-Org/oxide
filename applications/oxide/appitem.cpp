#include "appitem.h"

bool AppItem::ok(){
    return !_name.isEmpty() && !_call.isEmpty();
}

void AppItem::execute(){
    system(_call.toUtf8());
}
