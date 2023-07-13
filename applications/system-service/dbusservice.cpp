#include "dbusservice.h"

using namespace std::chrono;

DBusService* DBusService::singleton(){
    auto bus = QDBusConnection::systemBus();
    if(instance == nullptr){
        qApp->thread()->setObjectName("main"); // To make identifying threads from QDebug output easier
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
        sd_notify(0, "STATUS=startup");
        guiAPI->startup();
        appsAPI->startup();
        sd_notify(0, "STATUS=running");
        sd_notify(0, "READY=1");
    }
    return instance;
}


DBusService::DBusService(QObject* parent) : APIBase(parent), apis(), children(){
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "Initializing APIs", "info");
#endif
    Oxide::Sentry::sentry_transaction("dbus", "init", [this](Oxide::Sentry::Transaction* t){
        uint64_t time;
        int res = sd_watchdog_enabled(0, &time);
        if(res > 0){
            auto usec = microseconds(time);
            auto hrs = duration_cast<hours>(usec);
            auto mins = duration_cast<minutes>(usec - hrs);
            auto secs = duration_cast<seconds>(usec - hrs - mins);
            auto ms = duration_cast<milliseconds>(usec - hrs - mins - secs);
            qInfo() << "Watchdog notification expected every" << QString("%1:%2:%3.%4").arg(hrs.count()).arg(mins.count()).arg(secs.count()).arg(ms.count());
            usec = usec / 2;
            hrs = duration_cast<hours>(usec);
            mins = duration_cast<minutes>(usec - hrs);
            secs = duration_cast<seconds>(usec - hrs - mins);
            ms = duration_cast<milliseconds>(usec - hrs - mins - secs);
            qInfo() << "Watchdog scheduled for every " << QString("%1:%2:%3.%4").arg(hrs.count()).arg(mins.count()).arg(secs.count()).arg(ms.count());
            watchdogTimer = startTimer(duration_cast<milliseconds>(usec).count(), Qt::PreciseTimer);
            if(watchdogTimer == 0){
                qCritical() << "Watchdog timer failed to start";
            }else{
                qInfo() << "Watchdog timer running";
            }
        }else if(res < 0){
            qWarning() << "Failed to detect if watchdog timer is expected:" << strerror(-res);
        }else{
            qInfo() << "No watchdog timer required";
        }
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
                    .security = APISecurity::Open
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
    sd_notify(0, "STATUS=stopping");
    sd_notify(0, "STOPPING=1");
    emit aboutToQuit();
    appsAPI->shutdown();
    guiAPI->shutdown();
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
    QMutableListIterator i(children);
    while(i.hasNext()){
        auto child = i.next();
        i.remove();
        delete child;
    }
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
