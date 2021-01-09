#include "screenshot.h"
#include"screenapi.h"

bool Screenshot::hasPermission(QString permission, const char* sender){ return screenAPI->hasPermission(permission, sender); }
