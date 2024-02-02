#include "dbusservice.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "appsapi.h"
#include "systemapi.h"
#include "screenapi.h"
#include "notificationapi.h"
#include "buttonhandler.h"

DBusService* DBusService::singleton(){
    static DBusService* instance;
    if(instance == nullptr){
        qRegisterMetaType<QMap<QString, QDBusObjectPath>>();
        O_DEBUG("Creating DBusService instance");
        instance = new DBusService(qApp);
        connect(qApp, &QGuiApplication::aboutToQuit, [=]{
            if(instance == nullptr){
                return;
            }
            emit instance->aboutToQuit();
            O_DEBUG("Killing dbus service");
            delete instance;
            qApp->processEvents();
            instance = nullptr;
        });
        auto bus = QDBusConnection::systemBus();
        if(!bus.isConnected()){
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Failed to connect to system bus.", "error");
#endif
            qFatal("Failed to connect to system bus.");
        }
        QDBusConnectionInterface* interface = bus.interface();
        O_DEBUG("Registering service...");
        auto reply = interface->registerService(OXIDE_SERVICE);
        bus.registerService(OXIDE_SERVICE);
        if(!reply.isValid()){
            QDBusError ex = reply.error();
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Unable to register service", "error");
#endif
            qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
        }
        O_DEBUG("Registering object...");
        if(!bus.registerObject(OXIDE_SERVICE_PATH, instance, QDBusConnection::ExportAllContents)){
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Unable to register interface", "error");
#endif
            qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
        }
        connect(
            bus.interface(),
            SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            instance,
            SLOT(serviceOwnerChanged(QString,QString,QString))
        );
        O_DEBUG("Registered");
    }
    return instance;
}

DBusService::DBusService(QObject* parent) : APIBase(parent), apis(){
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
        });
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "Connecting button handler events", "info");
#endif
        Oxide::Sentry::sentry_span(t, "connect", "Connect events", []{
            if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM1){
                connect(buttonHandler, &ButtonHandler::leftHeld, systemAPI, &SystemAPI::leftAction);
                connect(buttonHandler, &ButtonHandler::homeHeld, systemAPI, &SystemAPI::homeAction);
                connect(buttonHandler, &ButtonHandler::rightHeld, systemAPI, &SystemAPI::rightAction);
            }
            connect(buttonHandler, &ButtonHandler::powerHeld, systemAPI, &SystemAPI::powerAction);
            connect(buttonHandler, &ButtonHandler::powerPress, systemAPI, &SystemAPI::suspend);
            connect(buttonHandler, &ButtonHandler::activity, systemAPI, &SystemAPI::activity);
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Connecting power events", "info");
#endif
            connect(powerAPI, &PowerAPI::chargerStateChanged, systemAPI, &SystemAPI::activity);
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Connecting system events", "info");
#endif
            connect(systemAPI, &SystemAPI::leftAction, appsAPI, []{
                if(notificationAPI->locked()){
                    return;
                }
                auto currentApplication = appsAPI->getApplication(appsAPI->currentApplicationNoSecurityCheck());
                if(currentApplication != nullptr && currentApplication->path() == appsAPI->lockscreenApplication().path()){
                    O_DEBUG("Left Action cancelled. On lockscreen");
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
    O_DEBUG("Removing all APIs");
    auto bus = QDBusConnection::systemBus();
    for(auto api : apis){
        api.instance->setEnabled(false);
        bus.unregisterObject(api.path);
        emit apiUnavailable(QDBusObjectPath(api.path));
        delete api.instance;
        delete api.dependants;
    }
    apis.clear();
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "APIs disconnected", "info");
#endif
}

void DBusService::setEnabled(bool enabled){ Q_UNUSED(enabled); }

QObject* DBusService::getAPI(QString name){
    if(!apis.contains(name)){
        return nullptr;
    }
    return apis[name].instance;
}

QQmlApplicationEngine* DBusService::engine(){ return m_engine; }

int DBusService::tarnishPid(){ return qApp->applicationPid(); }

QDBusObjectPath DBusService::requestAPI(QString name, QDBusMessage message) {
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", ("requestAPI() " + name).toStdString().c_str(), "query");
#endif
    if(!hasPermission(name)){
        return QDBusObjectPath("/");
    }
    if(!apis.contains(name)){
        return QDBusObjectPath("/");
    }
    auto api = apis[name];
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(api.path) == nullptr){
        bus.registerObject(api.path, api.instance, QDBusConnection::ExportAllContents);
    }
    if(!api.dependants->size()){
        O_DEBUG("Registering " << api.path);
        api.instance->setEnabled(true);
        emit apiAvailable(QDBusObjectPath(api.path));
    }
    api.dependants->append(message.service());
    return QDBusObjectPath(api.path);
}

void DBusService::releaseAPI(QString name, QDBusMessage message) {
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", ("releaseAPI() " + name).toStdString().c_str(), "query");
#endif
    if(!apis.contains(name)){
        return;
    }
    auto api = apis[name];
    auto client = message.service();
    api.dependants->removeAll(client);
    if(!api.dependants->size()){
        O_DEBUG("Unregistering " << api.path);
        api.instance->setEnabled(false);
        QDBusConnection::systemBus().unregisterObject(api.path, QDBusConnection::UnregisterNode);
        emit apiUnavailable(QDBusObjectPath(api.path));
    }
}

QVariantMap DBusService::APIs(){
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "APIs()", "query");
#endif
    QVariantMap result;
    for(auto key : apis.keys()){
        auto api = apis[key];
        if(api.dependants->size()){
            result[key] = QVariant::fromValue(api.path);
        }
    }
    return result;
}

void DBusService::startup(QQmlApplicationEngine* engine){
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "startup", "navigation");
#endif
    m_engine = engine;
    auto compositor = getCompositorDBus();
    compositor->setFlags(QString("connection/%1").arg(::getpid()), QStringList() << "system");
    notificationAPI->startup();
    appsAPI->startup();
}

void DBusService::serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
    Q_UNUSED(oldOwner);
    if(newOwner.isEmpty()){
        auto bus = QDBusConnection::systemBus();
        for(auto key : apis.keys()){
            auto api = apis[key];
            api.dependants->removeAll(name);
            if(!api.dependants->size() && bus.objectRegisteredAt(api.path) != nullptr){
                O_DEBUG("Automatically unregistering " << api.path);
                api.instance->setEnabled(false);
                bus.unregisterObject(api.path, QDBusConnection::UnregisterNode);
                apiUnavailable(QDBusObjectPath(api.path));
            }
        }
        systemAPI->uninhibitAll(name);
    }
}
#include "moc_dbusservice.cpp"
