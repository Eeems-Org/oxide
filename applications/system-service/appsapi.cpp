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
    int size = settings.beginReadArray("applications");
    for(int i = 0; i < size; ++i){
        settings.setArrayIndex(i);
        auto name = settings.value("name").toString();
        auto displayName = settings.value("displayName", name).toString();
        auto type = settings.value("type", Foreground).toInt();
        auto bin = settings.value("bin").toString();
        if(type < Foreground || type > Background || name.isEmpty() || bin.isEmpty()){
            continue;
        }
        auto app = new Application(getPath(name), reinterpret_cast<QObject*>(this));
        app->setConfig(QVariantMap {
            {"name", name},
            {"displayName", displayName},
            {"description", settings.value("description", displayName).toString()},
            {"bin", bin},
            {"type", type},
            {"flags", settings.value("flags", QStringList()).toStringList()},
            {"icon", settings.value("icon", "").toString()},
            {"onPause", settings.value("onPause", "").toString()},
            {"onResume", settings.value("onResume", "").toString()},
            {"onStop", settings.value("onStop", "").toString()},
            {"environment", settings.value("environment", QVariantMap()).toMap()},
        });
        applications.insert(name, app);
    }
    settings.endArray();
    readApplications();
    auto path = QDBusObjectPath(settings.value("startupApplication").toString());
    auto app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.oxide");
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
