#include "dbusservice.h"

DBusService* DBusService::singleton(){
    auto bus = QDBusConnection::systemBus();
    if(instance == nullptr){
        qRegisterMetaType<QMap<QString, QDBusObjectPath>>();
        qDebug() << "Creating DBusService instance";
        instance = new DBusService(nullptr);
        if(!bus.isConnected()){
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Failed to connect to system bus.", "error");
#endif
            qFatal("Failed to connect to system bus.");
        }
        QDBusConnectionInterface* interface = bus.interface();
        qDebug() << "Registering service...";
        auto reply = interface->registerService(OXIDE_SERVICE);
        bus.registerService(OXIDE_SERVICE);
        if(!reply.isValid()){
            QDBusError ex = reply.error();
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Unable to register service", "error");
#endif
            qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
        }
        qDebug() << "Registering object...";
        if(!bus.registerObject(OXIDE_SERVICE_PATH, instance, QDBusConnection::ExportAllContents)){
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Unable to register interface", "error");
#endif
            qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
        }
        connect(bus.interface(), &QDBusConnectionInterface::serviceOwnerChanged, instance, &DBusService::serviceOwnerChanged);
        qDebug() << "Registered";

#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "startup", "navigation");
#endif
        guiAPI->startup();
        appsAPI->startup();
    }
    return instance;
}


DBusService::DBusService(QObject* parent) : APIBase(parent), apis(), children(){
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "Initializing APIs", "info");
#endif
    Oxide::Sentry::sentry_transaction("dbus", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "apis", "Initialize APIs", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "wifi", "Initialize wifi API", [this]{
                apis.insert("wifi", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/wifi",
                    .dependants = new QStringList(),
                    .instance = new WifiAPI(this),
                });
            });
            Oxide::Sentry::sentry_span(s, "system", "Initialize system API", [this]{
                apis.insert("system", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/system",
                    .dependants = new QStringList(),
                    .instance = new SystemAPI(this),
                });
            });
            Oxide::Sentry::sentry_span(s, "power", "Initialize power API", [this]{
                apis.insert("power", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/power",
                    .dependants = new QStringList(),
                    .instance = new PowerAPI(this),
                });
            });
            Oxide::Sentry::sentry_span(s, "screen", "Initialize screen API", [this]{
                apis.insert("screen", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/screen",
                    .dependants = new QStringList(),
                    .instance = new ScreenAPI(this),
                });
            });
            Oxide::Sentry::sentry_span(s, "apps", "Initialize apps API", [this]{
                apis.insert("apps", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/apps",
                    .dependants = new QStringList(),
                    .instance = new AppsAPI(this),
                });
            });
            Oxide::Sentry::sentry_span(s, "notification", "Initialize notification API", [this]{
                apis.insert("notification", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/notification",
                    .dependants = new QStringList(),
                    .instance = new NotificationAPI(this),
                });
            });

            Oxide::Sentry::sentry_span(s, "gui", "Initialize GUI API", [this]{
                apis.insert("gui", APIEntry{
                    .path = QString(OXIDE_SERVICE_PATH) + "/gui",
                    .dependants = new QStringList(),
                    .instance = new GuiAPI(this),
                });
            });
        });
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "Connecting events", "info");
#endif
        Oxide::Sentry::sentry_span(t, "connect", "Connect events", [this]{
            connect(buttonHandler, &ButtonHandler::leftHeld, systemAPI, &SystemAPI::leftAction);
            connect(buttonHandler, &ButtonHandler::homeHeld, systemAPI, &SystemAPI::homeAction);
            connect(buttonHandler, &ButtonHandler::rightHeld, systemAPI, &SystemAPI::rightAction);
            connect(buttonHandler, &ButtonHandler::powerHeld, systemAPI, &SystemAPI::powerAction);
            connect(buttonHandler, &ButtonHandler::powerPress, systemAPI, &SystemAPI::suspend);
            connect(buttonHandler, &ButtonHandler::activity, systemAPI, &SystemAPI::activity);
            connect(powerAPI, &PowerAPI::chargerStateChanged, systemAPI, &SystemAPI::activity);
            connect(systemAPI, &SystemAPI::leftAction, appsAPI, []{
                if(notificationAPI->locked()){
                    return;
                }
                auto currentApplication = appsAPI->getApplication(appsAPI->currentApplicationNoSecurityCheck());
                if(currentApplication != nullptr && currentApplication->path() == appsAPI->lockscreenApplication().path()){
                    qDebug() << "Left Action cancelled. On lockscreen";
                    return;
                }
                if(!appsAPI->previousApplicationNoSecurityCheck()){
                    appsAPI->openDefaultApplication();
                }
            });
            connect(systemAPI, &SystemAPI::homeAction, appsAPI, &AppsAPI::openTaskManager);
            connect(systemAPI, &SystemAPI::bottomAction, appsAPI, &AppsAPI::openTaskSwitcher);
            connect(systemAPI, &SystemAPI::topAction, systemAPI, &SystemAPI::toggleSwipes);
        });
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "Cleaning up", "info");
#endif
        Oxide::Sentry::sentry_span(t, "cleanup", "Cleanup", [this]{
        auto bus = QDBusConnection::systemBus();
            for(auto api : apis){
                bus.unregisterObject(api.path);
            }
        });
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "APIs initialized", "info");
#endif
    });
}

DBusService::~DBusService(){
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "Disconnecting APIs", "info");
#endif
    qDebug() << "Deleting APIs";
    while(!apis.isEmpty()){
        auto name = apis.firstKey();
        qDebug() << "Deleting API" << name;
        auto api = apis.take(name);
        delete api.dependants;
        delete api.instance;
    }
    qDebug() << "All APIs removed";
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "APIs disconnected", "info");
#endif
}

bool DBusService::isEnabled(){
    auto reply = QDBusConnection::systemBus().interface()->registeredServiceNames();
    return reply.isValid() && reply.value().contains(OXIDE_SERVICE);
}

void DBusService::shutdown(){
    if(calledFromDBus()){
        return;
    }
    emit aboutToQuit();
    appsAPI->shutdown();
    guiAPI->shutdown();
    auto bus = QDBusConnection::systemBus();
    for(auto key : apis.keys()){
        auto api = apis[key];
        api.instance->setEnabled(false);
        bus.unregisterObject(api.path, QDBusConnection::UnregisterNode);
        emit apiUnavailable(QDBusObjectPath(api.path));
    }
    bus.unregisterService(OXIDE_SERVICE);
    delete instance;
    instance = nullptr;
    qApp->quit();
}

DBusService* DBusService::instance = nullptr;
