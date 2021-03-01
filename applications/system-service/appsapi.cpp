#include "appsapi.h"
#include "notificationapi.h"

AppsAPI::AppsAPI(QObject* parent)
: APIBase(parent),
  m_stopping(false),
  m_starting(true),
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
            qDebug() << "Auto starting" << app->name();
            app->launchNoSecurityCheck();
            if(app->type() == Backgroundable){
                qDebug() << "  Puasing auto started app" << app->name();
                app->pauseNoSecurityCheck();
            }
        }
    }
    auto app = getApplication(m_lockscreenApplication);
    if(app == nullptr){
        qDebug() << "Could not find lockscreen application";
        app = getApplication(m_startupApplication);
    }
    if(app == nullptr){
        qDebug() << "could not find startup application";
        return;
    }
    qDebug() << "Starting initial application" << app->name();
    app->launchNoSecurityCheck();
    m_starting = false;
}

bool AppsAPI::locked(){ return notificationAPI->locked(); }
