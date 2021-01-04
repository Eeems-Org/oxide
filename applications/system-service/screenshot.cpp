#include "screenshot.h"
#include"screenapi.h"

bool Screenshot::hasPermission(QString permission){ return screenAPI->hasPermission(permission); }
