#include "liboxide.h"
#include <sstream>
#include <fstream>

#ifdef SENTRY
std::string readFile(std::string path){
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
#endif
void sentry_setup_options(const char* name, char* argv[]){
#ifdef SENTRY
    sentry_options_t* options = sentry_options_new(); \
    sentry_options_set_dsn(options, "https://8d409799a9d640599cc66496fb87edf6@sentry.eeems.codes/2");
    sentry_options_set_auto_session_tracking(options, true);
    sentry_options_set_symbolize_stacktraces(options, true);
    sentry_options_set_environment(options, argv[0]);
#ifdef DEBUG
    sentry_options_set_debug(options, true);
#endif
    sentry_options_set_database_path(options, "/home/root/.cache/Eeems/sentry");
    sentry_options_set_release(options, (std::string(name) + "@2.3").c_str());
    sentry_init(options);
#else
    Q_UNUSED(name);
    Q_UNUSED(argv);
#endif
}
void sentry_setup_user(){
#ifdef SENTRY
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
    sentry_set_user(user);
#endif
}
void sentry_setup_context(){
#ifdef SENTRY
    std::string version = readFile("/etc/version");
    sentry_set_tag("os.version", version.c_str());
    sentry_value_t device = sentry_value_new_object();
    sentry_value_set_by_key(device, "machine-id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
    sentry_value_set_by_key(device, "version", sentry_value_new_string(readFile("/etc/version").c_str()));
    sentry_set_context("device", device);
#endif
}
void initSentry(const char* name, char* argv[]){
#ifdef SENTRY
    sentry_setup_options(name, argv); \
    sentry_setup_user(); \
    sentry_setup_context(); \
    sentry_set_transaction(name); \
    auto sentryClose = qScopeGuard([] { sentry_close(); });
#else
    Q_UNUSED(name);
    Q_UNUSED(argv);
    qWarning() << "Telemetry disabled";
#endif
}
void sentry_breadcrumb(const char* category, const char* message, const char* type, const char* level){
#ifdef SENTRY
    sentry_value_t crumb = sentry_value_new_breadcrumb(type, message);
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string(category));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string(level));
    sentry_add_breadcrumb(crumb);
#else
    Q_UNUSED(category);
    Q_UNUSED(message);
    Q_UNUSED(type);
    Q_UNUSED(level);
#endif
}

Liboxide::Liboxide()
{
}
