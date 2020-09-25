#include "appsapi.h"

AppsAPI::AppsAPI(QObject* parent)
: APIBase(parent),
  m_enabled(false),
  applications(),
  settings(reinterpret_cast<QObject*>(this)),
  m_startupApplication("/"),
  m_sleeping(false) {
    singleton(this);
    setup_unix_signal_handlers();
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
        auto displayName = settings.value("displayname", name).toString();
        auto app = new Application(getPath(name), reinterpret_cast<QObject*>(this));
        app->setConfig(QVariantMap {
            {"name", name},
            {"displayName", displayName},
            {"description", settings.value("description", displayName).toString()},
            {"bin", settings.value("bin").toString()},
            {"type", settings.value("type", Foreground).toInt()},
            {"flags", settings.value("flags", QStringList()).toStringList()},
            {"icon", settings.value("icon", "").toString()},
            {"onPause", settings.value("onPause", "").toString()},
            {"onResume", settings.value("onResume", "").toString()},
            {"onStop", settings.value("onStop", "").toString()},
        });
        applications.insert(name, app);
    }
    settings.endArray();
    if(!applications.size()){
        // TODO load from draft config files
    }
    if(!applications.contains("xochitl")){
        auto app = new Application(getPath("xochitl"), reinterpret_cast<QObject*>(this));
        app->setConfig(QVariantMap {
            {"name", "xochitl"},
            {"displayName", "Xochitl"},
            {"description", "reMarkable default application"},
            {"bin", "/usr/bin/xochitl"},
            {"type", Foreground},
            {"flags", QStringList() << "system"},
            {"icon", "/opt/etc/draft/icons/xochitl.png"}
        });
        applications[app->name()] = app;
        emit applicationRegistered(app->qPath());
    }
    if(!applications.contains("codes.eeems.erode")){
        auto app = new Application(getPath("codes.eeems.erode"), reinterpret_cast<QObject*>(this));
        app->setConfig(QVariantMap {
            {"name", "codes.eeems.erode"},
            {"displayName", "Process Manager"},
            {"description", "List and kill running processes"},
            {"bin", "/opt/bin/erode"},
            {"type", Foreground},
            {"flags", QStringList() << "system"},
        });
        applications[app->name()] = app;
        emit applicationRegistered(app->qPath());
    }
    if(!applications.contains("codes.eeems.fret")){
        auto app = new Application(getPath("codes.eeems.fret"), reinterpret_cast<QObject*>(this));
        app->setConfig(QVariantMap {
            {"name", "codes.eeems.fret"},
            {"displayName", "Screenshot daemon"},
            {"description", "Handles taking screenshots"},
            {"bin", "/opt/bin/fret"},
            {"type", Background},
            {"flags", QStringList() << "system" << "hidden" << "autoStart"},
        });
        applications[app->name()] = app;
        emit applicationRegistered(app->qPath());
    }
    auto path = QDBusObjectPath(settings.value("startupApplication").toString());
    auto app = getApplication(path);
    if(app == nullptr){
        app = getApplication("codes.eeems.oxide");
        if(app == nullptr){
            app = new Application(getPath("codes.eeems.oxide"), reinterpret_cast<QObject*>(this));
            app->setConfig(QVariantMap {
                {"name", "codes.eeems.oxide"},
                {"displayName", "Launcher"},
                {"description", "Application launcher"},
                {"bin", "/opt/bin/oxide"},
                {"type", Foreground},
                {"flags", QStringList() << "system" << "hidden"},
            });
            applications[app->name()] = app;
            emit applicationRegistered(app->qPath());
        }
        path = app->qPath();
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
    getApplication(m_startupApplication)->launch();
}
