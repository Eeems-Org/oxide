#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

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

#include "powerapi.h"
#include "wifiapi.h"
#include "appsapi.h"
#include "systemapi.h"
#include "screenapi.h"
#include "notificationapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

#define dbusService DBusService::singleton()

using namespace std;
using namespace Oxide::Sentry;

struct APIEntry {
    QString path;
    QStringList* dependants;
    APIBase* instance;
};

struct ChildEntry {
    std::string service;
    qint64 pid;
    std::string name;
    int stdout;
    int stderr;
    int fb;
    int fbWidth;
    int fbHeight;
    int eventRead;
    int eventWrite;
    void* fbData = nullptr;
    QImage* fbImage = nullptr;
    std::string uniqueName() const{
        return QString("%1-%2-%3").arg(service.c_str()).arg(name.c_str()).arg(pid).toStdString();
    }
    ssize_t size() const {
        return fbWidth * fbHeight;
    }
    QImage* frameBuffer() {
        if(fbImage){
            return fbImage;
        }
        if(fb == -1){
            return nullptr;
        }
        if(fbData == nullptr || fbData == MAP_FAILED){
            fbData = mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_SYNC | MAP_HUGETLB, fb, 0);
        }
        if(fbData == MAP_FAILED){
            return nullptr;
        }
        fbImage = new QImage((uchar*)fbData, fbWidth, fbHeight, QImage::Format_Mono);
        return fbImage;
    }
};

class DBusService : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GENERAL_INTERFACE)
    Q_PROPERTY(int tarnishPid READ tarnishPid)

public:
    static DBusService* singleton(){
        static DBusService* instance;
        if(instance == nullptr){
            qRegisterMetaType<QMap<QString, QDBusObjectPath>>();
            qDebug() << "Creating DBusService instance";
            instance = new DBusService(qApp);
            connect(qApp, &QGuiApplication::aboutToQuit, [=]{
                if(instance == nullptr){
                    return;
                }
                emit instance->aboutToQuit();
                qDebug() << "Killing dbus service ";
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
            connect(bus.interface(), SIGNAL(serviceOwnerChanged(QString,QString,QString)),
                    instance, SLOT(serviceOwnerChanged(QString,QString,QString)));
            qDebug() << "Registered";
        }
        return instance;
    }
    DBusService(QObject* parent) : APIBase(parent), apis(), children(){
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
                connect(buttonHandler, &ButtonHandler::leftHeld, systemAPI, &SystemAPI::leftAction);
                connect(buttonHandler, &ButtonHandler::homeHeld, systemAPI, &SystemAPI::homeAction);
                connect(buttonHandler, &ButtonHandler::rightHeld, systemAPI, &SystemAPI::rightAction);
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
    ~DBusService(){
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "Disconnecting APIs", "info");
#endif
        qDebug() << "Removing all APIs";
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
    void setEnabled(bool enabled){ Q_UNUSED(enabled); }

    QObject* getAPI(QString name){
        if(!apis.contains(name)){
            return nullptr;
        }
        return apis[name].instance;
    }

    int tarnishPid(){ return qApp->applicationPid(); }

    Q_INVOKABLE void registerChild(qint64 childPid, QString name, QDBusUnixFileDescriptor stdout, QDBusUnixFileDescriptor stderr, QDBusMessage message){
        Q_UNUSED(childPid)
        if(!QDBusUnixFileDescriptor::isSupported()){
            qCritical("QDBusUnixFileDescriptor is not supported");
            ::kill(childPid, SIGTERM);
        }
        if(!stdout.isValid()){
            O_WARNING("stdout passed in by" << childPid << "is invalid");
            ::kill(childPid, SIGTERM);
        }
        if(!stderr.isValid()){
            O_WARNING("stderr passed in by" << childPid << "is invalid");
            ::kill(childPid, SIGTERM);
        }
        qDebug() << "registerChild" << childPid << name;
        children.append(ChildEntry{
            .service = message.service().toStdString(),
            .pid = childPid,
            .name = name.toStdString(),
            .stdout = dup(stdout.fileDescriptor()),
            .stderr = dup(stderr.fileDescriptor()),
            .fb = -1,
            .fbWidth = -1,
            .fbHeight = -1,
            .eventRead = -1,
            .eventWrite = -1
        });
    }
    Q_INVOKABLE bool hasFrameBuffer(QDBusMessage message){ return getFrameBuffer(message).isValid(); }

    Q_INVOKABLE QDBusUnixFileDescriptor getFrameBuffer(QDBusMessage message){
        auto service = message.service().toStdString();
        QMutableListIterator<ChildEntry> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child.service != service){
                continue;
            }
            if(child.fb == -1){
                break;
            }
            return QDBusUnixFileDescriptor(child.fb);
        }
        return QDBusUnixFileDescriptor();
    }

    Q_INVOKABLE QDBusUnixFileDescriptor createFrameBuffer(int width, int height, QDBusMessage message){
        auto service = message.service().toStdString();
        QMutableListIterator<ChildEntry> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child.service != service){
                continue;
            }
            if(child.fb != -1){
                O_WARNING("Framebuffer already exists");
                break;
            }
            int fd = memfd_create(child.uniqueName().c_str(), MFD_ALLOW_SEALING | MFD_HUGETLB);
            if(fd == -1){
                O_WARNING("Unable to open memfd for framebuffer:" << strerror(errno));
                break;
            }
            if(ftruncate(fd, width * height)){
                close(fd);
                O_WARNING("Unable to truncate memfd for framebuffer:" << strerror(errno));
                break;
            }
            int flags = fcntl(fd, F_GET_SEALS);
            if(fcntl(fd, F_ADD_SEALS, flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW)){
                close(fd);
                O_WARNING("Unable to seal memfd for framebuffer:" << strerror(errno));
                break;
            }
            child.fb = fd;
            child.fbWidth = width;
            child.fbHeight = height;
            return QDBusUnixFileDescriptor(child.fb);
        }
        return QDBusUnixFileDescriptor();
    }

    Q_INVOKABLE void enableFrameBuffer(){
        // TODO start rendering framebuffer
    }

    Q_INVOKABLE QDBusUnixFileDescriptor getEventPipe(QDBusMessage message){
        auto service = message.service().toStdString();
        QMutableListIterator<ChildEntry> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child.service != service){
                continue;
            }
            if(child.eventRead != -1){
                return QDBusUnixFileDescriptor(child.eventRead);
            }
            int fds[2];
            if(pipe2(fds, O_DIRECT) == -1){
                O_WARNING("Unable to open events pipe:" << strerror(errno));
                break;
            }
            child.eventRead = fds[0];
            child.eventWrite = fds[1];
            return QDBusUnixFileDescriptor(child.eventRead);
        }
        return QDBusUnixFileDescriptor();
    }

    Q_INVOKABLE void enableEventPipe(QDBusMessage message){
        auto service = message.service().toStdString();
        QMutableListIterator<ChildEntry> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child.service != service || child.eventRead == -1){
                continue;
            }
            if(close(child.eventRead)){
                O_WARNING("Failed to close events write pipe" << strerror(errno));
            }else{
                child.eventRead = -1;
            }
            // TODO - start emitting events
        }
    }

    Q_INVOKABLE void unregisterChild(QDBusMessage message){ unregisterChild(message.service().toStdString()); }

public slots:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message) {
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
            qDebug() << "Registering " << api.path;
            api.instance->setEnabled(true);
            emit apiAvailable(QDBusObjectPath(api.path));
        }
        api.dependants->append(message.service());
        return QDBusObjectPath(api.path);
    }
    Q_NOREPLY void releaseAPI(QString name, QDBusMessage message) {
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
            qDebug() << "Unregistering " << api.path;
            api.instance->setEnabled(false);
            QDBusConnection::systemBus().unregisterObject(api.path, QDBusConnection::UnregisterNode);
            emit apiUnavailable(QDBusObjectPath(api.path));
        }
    }
    QVariantMap APIs(){
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

    void startup(){
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "startup", "navigation");
#endif
        appsAPI->startup();
    }


signals:
    void apiAvailable(QDBusObjectPath api);
    void apiUnavailable(QDBusObjectPath api);
    void aboutToQuit();

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
        Q_UNUSED(oldOwner);
        if(!newOwner.isEmpty()){
            return;
        }
        auto bus = QDBusConnection::systemBus();
        for(auto key : apis.keys()){
            auto api = apis[key];
            api.dependants->removeAll(name);
            if(!api.dependants->size() && bus.objectRegisteredAt(api.path) != nullptr){
                qDebug() << "Automatically unregistering " << api.path;
                api.instance->setEnabled(false);
                bus.unregisterObject(api.path, QDBusConnection::UnregisterNode);
                apiUnavailable(QDBusObjectPath(api.path));
            }
        }
        unregisterChild(name.toStdString());
        systemAPI->uninhibitAll(name);
    }

private:
    QMap<QString, APIEntry> apis;
    QList<ChildEntry> children;
    void unregisterChild(std::string service){
        QMutableListIterator<ChildEntry> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child.service != service){
                continue;
            }
            i.remove();
            if(child.fb != -1 && close(child.fb)){
                O_WARNING("Failed to close framebuffer" << strerror(errno));
            }
            if(child.eventRead != -1 && close(child.eventRead)){
                O_WARNING("Failed to close event read pipe" << strerror(errno));
            }
            if(child.eventWrite != -1 && close(child.eventWrite)){
                O_WARNING("Failed to close event write pipe" << strerror(errno));
            }

        }
    }
};

#endif // DBUSSERVICE_H
