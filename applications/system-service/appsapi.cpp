#include "appsapi.h"
#include "notificationapi.h"

AppsAPI::AppsAPI(QObject* parent)
: APIBase(parent),
  m_stopping(false),
  m_enabled(false),
  applications(),
  previousApplications(),
  settings(this),
  m_startupApplication("/"),
  m_lockscreenApplication("/"),
  m_processManagerApplication("/"),
  m_taskSwitcherApplication("/"),
  m_sleeping(false) {
    singleton(this);
    SignalHandler::setup_unix_signal_handlers();
    qDBusRegisterMetaType<QMap<QString,QDBusObjectPath>>();
    qDBusRegisterMetaType<QDBusObjectPath>();
    settings.sync();
    auto version = settings.value("version", 0).toInt();
    if(version < OXIDE_SETTINGS_VERSION){
        migrate(&settings, version);
    }
    readApplications();

    auto path = QDBusObjectPath(settings.value("lockscreenApplication").toString());
    auto app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.decay");
        if(app != nullptr){
            path = app->qPath();
        }
    }
    m_lockscreenApplication = path;

    path = QDBusObjectPath(settings.value("startupApplication").toString());
    app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.oxide");
        if(app != nullptr){
            path = app->qPath();
        }
    }
    m_startupApplication = path;

    path = QDBusObjectPath(settings.value("processManagerApplication").toString());
    app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.erode");
        if(app != nullptr){
            path = app->qPath();
        }
    }
    m_processManagerApplication= path;

    path = QDBusObjectPath(settings.value("processManagerApplication").toString());
    app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.corrupt");
        if(app != nullptr){
            path = app->qPath();
        }
    }
    m_taskSwitcherApplication= path;
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
    auto app = getApplication(m_lockscreenApplication);
    if(app == nullptr){
        app = getApplication(m_startupApplication);
    }
    if(app != nullptr){
        app->launch();
    }
}

bool AppsAPI::locked(){ return notificationAPI->locked(); }
