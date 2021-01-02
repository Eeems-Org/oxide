#include "appsapi.h"

AppsAPI::AppsAPI(QObject* parent)
: APIBase(parent),
  m_stopping(false),
  m_enabled(false),
  applications(),
  settings(this),
  m_startupApplication("/"),
  m_sleeping(false) {
    singleton(this);
    SignalHandler::setup_unix_signal_handlers();
    qDBusRegisterMetaType<QMap<QString,QDBusObjectPath>>();
    qDBusRegisterMetaType<QDBusObjectPath>();
    settings.sync();
    auto version = settings.value("version", OXIDE_SETTINGS_VERSION).toInt();
    if(version < OXIDE_SETTINGS_VERSION){
        migrate(&settings, version);
    }
    readApplications();
    auto path = QDBusObjectPath(settings.value("startupApplication").toString());
    auto app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.decay");
        if(app != nullptr){
            path = app->qPath();
        }
    }
    m_startupApplication = path;
}

void AppsAPI::startup(){
    for(auto app : applications){
        if(app->autoStart()){
            app->launch();
            if(app->type() == Backgroundable){
                app->pause();
            }
        }
    }
    auto app = getApplication(m_startupApplication);
    if(app != nullptr){
        app->launch();
    }
}
