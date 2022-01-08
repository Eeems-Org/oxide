#ifndef LIBOXIDE_H
#define LIBOXIDE_H

#include "liboxide_global.h"
#include <sentry.h>
#include <QDebug>
#include <QScopeGuard>


LIBOXIDE_EXPORT void initSentry(const char* name, char* argv[]);
LIBOXIDE_EXPORT void sentry_breadcrumb(const char* category, const char* message, const char* type = "default", const char* level = "info");
class LIBOXIDE_EXPORT Liboxide
{
public:
    Liboxide();
};

#endif // LIBOXIDE_H
