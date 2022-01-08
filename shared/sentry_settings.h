#ifndef SENTRY_SETTINGS_H
#define SENTRY_SETTINGS_H

#include <sentry.h>
#include <QDebug>
#include <QScopeGuard>

void sentry_setup_options(const char* name, char* argv[]);
void sentry_setup_user();
void sentry_setup_context();
void sentry_breadcrumb(const char* category, const char* message, const char* type = "default", const char* level = "info");

#ifdef SENTRY

#define initSentry(name, argv) \
    sentry_setup_options(name, argv); \
    sentry_setup_user(); \
    sentry_setup_context(); \
    sentry_set_transaction(name); \
    auto sentryClose = qScopeGuard([] { sentry_close(); }); \
    qDebug() << "Telemetry is enabled";

#else
#define initSentry(name, argv) qWarning() << "Telemetry disabled";
#endif

#endif // SENTRY_SETTINGS_H
