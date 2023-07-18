#include "dbusservice.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "appsapi.h"
#include "systemapi.h"
#include "screenapi.h"
#include "notificationapi.h"
#include "guiapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QLocalServer>
#include <fstream>
#include <liboxide.h>
#include <systemd/sd-daemon.h>

using namespace std::chrono;

DBusService* DBusService::__singleton(){
    auto bus = QDBusConnection::systemBus();
    if(instance == nullptr){
        qApp->thread()->setObjectName("main"); // To make identifying threads from QDebug output easier
        O_INFO("Creating DBusService instance");
        instance = new DBusService(nullptr);
        if(!bus.isConnected()){
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Failed to connect to system bus.", "error");
#endif
            qFatal("Failed to connect to system bus.");
        }
        QDBusConnectionInterface* interface = bus.interface();
        O_INFO("Registering service...");
        auto reply = interface->registerService(OXIDE_SERVICE);
        bus.registerService(OXIDE_SERVICE);
        if(!reply.isValid()){
            QDBusError ex = reply.error();
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Unable to register service", "error");
#endif
            qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
        }
        O_INFO("Registering object...");
        if(!bus.registerObject(OXIDE_SERVICE_PATH, instance, QDBusConnection::ExportAllContents)){
#ifdef SENTRY
            sentry_breadcrumb("dbusservice", "Unable to register interface", "error");
#endif
            qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
        }
        connect(bus.interface(), &QDBusConnectionInterface::serviceOwnerChanged, instance, &DBusService::serviceOwnerChanged);
        O_INFO("Registered");

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
                    O_INFO("Left Action cancelled. On lockscreen");
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
    O_INFO("Deleting APIs");
    while(!apis.isEmpty()){
        auto name = apis.firstKey();
        O_INFO("Deleting API" << name);
        auto api = apis.take(name);
        delete api.dependants;
        delete api.instance;
    }
    O_INFO("All APIs removed");
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", "APIs disconnected", "info");
#endif
}

void DBusService::setEnabled(bool enabled){ Q_UNUSED(enabled); }

bool DBusService::isEnabled(){
    auto reply = QDBusConnection::systemBus().interface()->registeredServiceNames();
    return reply.isValid() && reply.value().contains(OXIDE_SERVICE);
}

bool DBusService::shuttingDown(){ return m_shuttingDown; }

QObject* DBusService::getAPI(QString name){
    if(!apis.contains(name)){
        return nullptr;
    }
    return apis[name].instance;
}

int DBusService::tarnishPid(){ return qApp->applicationPid(); }

QDBusUnixFileDescriptor DBusService::registerChild(){
    auto childPid = getSenderPid();
    auto childPgid = getSenderPgid();
    O_INFO("registerChild::" << childPid << childPgid);
    for(auto child : children){
        if(child->pid() == childPid){
            O_INFO("registerChild::Found existing");
            return QDBusUnixFileDescriptor(child->socket()->socketDescriptor());
        }
    }
    auto child = new ChildEntry(this, childPid, childPgid);
    if(!child->isValid()){
        O_INFO("registerChild::Not valid");
        child->deleteLater();
        return QDBusUnixFileDescriptor();
    }
    connect(child, &ChildEntry::finished, this, [this, child]{ unregisterChild(child->pid()); });
    children.append(child);
    O_INFO("registerChild::success");
    return QDBusUnixFileDescriptor(child->socket()->socketDescriptor());
}

void DBusService::unregisterChild(){ unregisterChild(getSenderPid()); }

bool DBusService::isChild(pid_t pid){
    for(auto child : children){
        if(child->pid() == pid){
            return true;
        }
    }
    return false;
}

bool DBusService::isChildGroup(pid_t pgid){
    for(auto child : children){
        if(child->pgid() == pgid){
            return true;
        }
    }
    return false;
}

QDBusObjectPath DBusService::requestAPI(QString name, QDBusMessage message) {
#ifdef SENTRY
    sentry_breadcrumb("dbusservice", ("requestAPI() " + name).toStdString().c_str(), "query");
#endif
    if(!apis.contains(name)){
        return QDBusObjectPath("/");
    }
    auto api = apis[name];
    switch(api.security){
        case APISecurity::Default:
            if(!hasPermission(name)){
                return QDBusObjectPath("/");
            }
            break;
        case APISecurity::Secure:
            if(!hasPermissionStrict(name)){
                return QDBusObjectPath("/");
            }
            break;
        case APISecurity::Open:
            break;
        default:
            qWarning() << "Unknown APISecurity" << api.security;
            return QDBusObjectPath("/");
    }
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(api.path) == nullptr){
        bus.registerObject(api.path, api.instance, QDBusConnection::ExportAllContents);
    }
    if(!api.dependants->size()){
        O_INFO("Registering " << api.path);
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
        O_INFO("Unregistering " << api.path);
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

void DBusService::shutdown(){
    if(calledFromDBus()){
        return;
    }
    if(m_shuttingDown){
        kill(getpid(), SIGKILL);
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
    O_INFO("Stopping threads" << buttonHandler << touchHandler << wacomHandler);
    buttonHandler->quit();
    touchHandler->quit();
    wacomHandler->quit();
    QDeadlineTimer deadline(1000);
    if(!buttonHandler->wait(deadline)){
        O_WARNING("Terminated thread" << buttonHandler);
        buttonHandler->terminate();
        buttonHandler->wait();
    }
    if(!touchHandler->wait(deadline)){
        O_WARNING("Terminated thread" << touchHandler);
        touchHandler->terminate();
        touchHandler->wait();
    }
    if(!wacomHandler->wait(deadline)){
        O_WARNING("Terminated thread" << wacomHandler);
        wacomHandler->terminate();
        wacomHandler->wait();
    }
    delete instance;
    instance = nullptr;
    qApp->quit();
}

void DBusService::timerEvent(QTimerEvent* event){
    if(event->timerId() == watchdogTimer){
        O_DEBUG("Watchdog keepalive sent");
        sd_notify(0, "WATCHDOG=1");
    }
}

void DBusService::serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
    Q_UNUSED(oldOwner);
    // TODO - replace with QDBusServiceWatcher
    if(!newOwner.isEmpty()){
        return;
    }
    auto bus = QDBusConnection::systemBus();
    for(auto key : apis.keys()){
        auto api = apis[key];
        api.dependants->removeAll(name);
        if(!api.dependants->size() && bus.objectRegisteredAt(api.path) != nullptr){
            O_INFO("Automatically unregistering " << api.path);
            api.instance->setEnabled(false);
            bus.unregisterObject(api.path, QDBusConnection::UnregisterNode);
            emit apiUnavailable(QDBusObjectPath(api.path));
        }
    }
    systemAPI->uninhibitAll(name);
}

void DBusService::unregisterChild(pid_t pid){
    O_DEBUG("unregisterCHild" << pid << "requested");
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
    QMutableListIterator<ChildEntry*> i(children);
    while(i.hasNext()){
        auto child = i.next();
        if(child->pid() != pid){
            continue;
        }
        O_DEBUG("unregisterChild" << child->pid() << child->pgid());
        guiAPI->closeWindows(child->pid());
        i.remove();
        child->close();
        child->deleteLater();
    }
}

DBusService* DBusService::instance = nullptr;
bool DBusService::m_shuttingDown = false;
