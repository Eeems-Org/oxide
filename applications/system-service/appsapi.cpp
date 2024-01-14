#include <liboxide.h>

#include "appsapi.h"
#include "notificationapi.h"

using namespace Oxide;

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
    Oxide::Sentry::sentry_transaction("apps", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "start", "Launching initial application", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "singleton", "Setup singleton", [this]{
                singleton(this);
            });
            Oxide::Sentry::sentry_span(s, "signals", "Setup unix signal handlers", []{
                SignalHandler::setup_unix_signal_handlers();
            });
            qDBusRegisterMetaType<QMap<QString,QDBusObjectPath>>();
            qDBusRegisterMetaType<QDBusObjectPath>();
            Oxide::Sentry::sentry_span(s, "sync", "Sync settings", [this]{
                settings.sync();
            });
            auto version = settings.value("version", 0).toInt();
            if(version < OXIDE_SETTINGS_VERSION){
                Oxide::Sentry::sentry_span(s, "migrate", "Migrate to latest version", [this, version]{
                    migrate(&settings, version);
                });
            }
        });
        Oxide::Sentry::sentry_span(t, "application", "Read applications from disk", [this]{
            readApplications();
        });
        Oxide::Sentry::sentry_span(t, "setup", "Setup lockscreen, startup, process manager, task switcher", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "lockscreenApplication", "Determine what the lockscreen application is", [this]{
                auto path = QDBusObjectPath(settings.value("lockscreenApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    app = getApplication("codes.eeems.decay");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_lockscreenApplication = path;
            });
            Oxide::Sentry::sentry_span(s, "startupApplication", "Determine what the startup application is", [this]{
                auto path = QDBusObjectPath(settings.value("startupApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    app = getApplication("codes.eeems.oxide");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_startupApplication = path;
            });
            Oxide::Sentry::sentry_span(s, "processManagerApplication", "Determine what the process manager application is", [this]{
                auto path = QDBusObjectPath(settings.value("processManagerApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    app = getApplication("codes.eeems.erode");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_processManagerApplication= path;
            });
            Oxide::Sentry::sentry_span(s, "taskSwitcherApplication", "Determine what the task switcher application is", [this]{
                auto path = QDBusObjectPath(settings.value("taskSwitcherApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    path = QDBusObjectPath(settings.value("processManagerApplication").toString());
                    app = getApplication(path);
                }
                if(app == nullptr){
                    app = getApplication("codes.eeems.corrupt");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_taskSwitcherApplication= path;
            });
        });
    });
}

void AppsAPI::startup(){
    Oxide::Sentry::sentry_transaction("apps", "startup", [this](Oxide::Sentry::Transaction* t){
        if(applications.isEmpty()){
            qDebug() << "No applications found";
            notificationAPI->errorNotification(_noApplicationsMessage);
            return;
        }
        Oxide::Sentry::sentry_span(t, "autoStart", "Launching auto start applications", [this](Oxide::Sentry::Span* s){
            for(auto app : applications){
                if(app->autoStart()){
                    qDebug() << "Auto starting" << app->name();
                    Oxide::Sentry::sentry_span(s, app->name().toStdString(), "Launching application", [app]{
                        app->launchNoSecurityCheck();
                        if(app->type() == Backgroundable){
                            qDebug() << "  Pausing auto started app" << app->name();
                            app->pauseNoSecurityCheck();
                        }
                    });
                }
            }
        });
        Oxide::Sentry::sentry_span(t, "start", "Launching initial application", [this]{
            auto app = getApplication(m_lockscreenApplication);
            if(app == nullptr){
                qDebug() << "Could not find lockscreen application";
                app = getApplication(m_startupApplication);
            }
            if(app == nullptr){
                qDebug() << "Could not find startup application";
                qDebug() << "Using xochitl due to invalid configuration";
                app = getApplication("xochitl");
            }
            if(app == nullptr){
                qDebug() << "Could not find xochitl";
                qWarning() << "Using the first application in the list due to invalid configuration";
                app = applications.first();
            }
            qDebug() << "Starting initial application" << app->name();
            app->launchNoSecurityCheck();
            ensureForegroundApp();
        });
    });
}

void AppsAPI::resumeIfNone(){
    if(m_stopping || m_starting){
        return;
    }
    for(auto app : applications){
        if(app->stateNoSecurityCheck() == Application::InForeground){
            return;
        }
    }
    if(previousApplicationNoSecurityCheck()){
        return;
    }
    auto app = getApplication(m_startupApplication);
    if(app == nullptr){
        app = getApplication("xochitl");
    }
    if(app == nullptr){
        if(applications.isEmpty()){
            qDebug() << "No applications found";
            notificationAPI->errorNotification(_noApplicationsMessage);
            return;
        }
        app = applications.first();
    }
    app->launchNoSecurityCheck();
    m_starting = true;
    ensureForegroundApp();
}

bool AppsAPI::locked(){ return notificationAPI->locked(); }

void AppsAPI::ensureForegroundApp() {
    QTimer::singleShot(300, [this]{
        m_starting = false;
        auto path = appsAPI->currentApplicationNoSecurityCheck();
        if(path.path() == "/"){
            notificationAPI->errorNotification(_noForegroundAppMessage);
            return;
        }
        auto app = appsAPI->getApplication(path);
        if(app == nullptr || app->state() == Application::Inactive){
            notificationAPI->errorNotification(_noForegroundAppMessage);
            return;
        }
    });
}

#include "moc_appsapi.cpp"
