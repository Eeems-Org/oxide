#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QImage>
#include <QQuickItem>

#include <epframebuffer.h>
#include <liboxide.h>

#include "dbusservice_interface.h"
#include "screenapi_interface.h"
#include "appsapi_interface.h"
#include "application_interface.h"

#include "screenprovider.h"
#include "signalhandler.h"
#include "appitem.h"

using namespace codes::eeems::oxide1;

#define CORRUPT_SETTINGS_VERSION 1

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
enum WifiState { WifiUnknown, WifiOff, WifiDisconnected, WifiOffline, WifiOnline};

class Controller : public QObject {
    Q_OBJECT
public:
    Controller(QObject* parent, ScreenProvider* screenProvider)
    : QObject(parent), settings(this), applications() {
        this->screenProvider = screenProvider;
        auto bus = QDBusConnection::systemBus();
        qDebug() << "Waiting for tarnish to start up...";
        while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            struct timespec args{
                .tv_sec = 1,
                .tv_nsec = 0,
            }, res;
            nanosleep(&args, &res);
        }
        api = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, this);

        SignalHandler::setup_unix_signal_handlers();
        connect(signalHandler, &SignalHandler::sigUsr1, this, &Controller::sigUsr1);
        connect(signalHandler, &SignalHandler::sigUsr2, this, &Controller::sigUsr2);

        qDebug() << "Requesting screen API...";
        QDBusObjectPath path = api->requestAPI("screen");
        if(path.path() == "/"){
            qDebug() << "Unable to get screen API";
            throw "";
        }
        screenApi = new Screen(OXIDE_SERVICE, path.path(), bus, this);

        qDebug() << "Requesting apps API...";
        path = api->requestAPI("apps");
        if(path.path() == "/"){
            qDebug() << "Unable to get apps API";
            throw "";
        }
        appsApi = new Apps(OXIDE_SERVICE, path.path(), bus, this);
        connect(appsApi, &Apps::applicationUnregistered, this, &Controller::unregisterApplication);
        connect(appsApi, &Apps::applicationRegistered, this, &Controller::registerApplication);
        connect(appsApi, &Apps::applicationLaunched, this, &Controller::reload);
        connect(appsApi, &Apps::applicationExited, this, &Controller::reload);

        settings.sync();
        auto version = settings.value("version", 0).toInt();
        if(version < CORRUPT_SETTINGS_VERSION){
            migrate(&settings, version);
        }
        saveScreen();
    }
    ~Controller(){}

    Q_INVOKABLE void startup(){
        qDebug() << "Running controller startup";
        emit reload();
        QTimer::singleShot(10, [this]{
            setState("loaded");
        });
    }
    Q_INVOKABLE void previousApplication(){
        auto reply = appsApi->previousApplication();
        while(!reply.isFinished()){
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
        }
        if(reply.isError() || !reply.value()){
            launchStartupApp();
        }
    }
    Q_INVOKABLE void launchStartupApp(){
        auto reply = appsApi->openDefaultApplication();
        while(!reply.isFinished()){
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
        }
    }
    Q_INVOKABLE void launchTaskManager(){
        auto reply = appsApi->openTaskManager();
        while(!reply.isFinished()){
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
        }
    }
    Q_INVOKABLE QList<QObject*> getApps(){
        auto bus = QDBusConnection::systemBus();
        auto running = appsApi->runningApplications();
        auto paused = appsApi->pausedApplications();
        for(auto key : paused.keys()){
            if(running.contains(key)){
                continue;
            }
            running.insert(key, paused[key]);
        }
        auto runningApplications = appsApi->runningApplications().values();
        runningApplications.append(appsApi->pausedApplications().values());
        for(auto item : runningApplications){
            auto path = item.value<QDBusObjectPath>().path();
            Application app(OXIDE_SERVICE, path, bus, this);
            qDebug() << app.name() << app.hidden();
            if(app.hidden()){
                continue;
            }
            auto name = app.name();
            auto appItem = getApplication(name);
            if(appItem == nullptr){
                qDebug() << name;
                appItem = new AppItem(this);
                applications.append(appItem);
            }
            auto displayName = app.displayName();
            if(displayName.isEmpty()){
                displayName = name;
            }
            appItem->setProperty("path", path);
            appItem->setProperty("name", name);
            appItem->setProperty("displayName", displayName);
            appItem->setProperty("desc", app.description());
            appItem->setProperty("call", app.bin());
            appItem->setProperty("running", running.contains(name));
            auto icon = app.icon();
            if(!icon.isEmpty() && QFile(icon).exists()){
                    appItem->setProperty("imgFile", "file:" + icon);
            }
            if(!appItem->ok()){
                qDebug() << "Invalid item" << appItem->property("name").toString();
                applications.removeAll(appItem);
                delete appItem;
            }
        }
        auto previousApplications = appsApi->previousApplications();
        // Sort by name
        std::sort(applications.begin(), applications.end(), [=](const QObject* a, const QObject* b) -> bool {
            auto aName = a->property("name").toString();
            auto bName = b->property("name").toString();
            int aIndex = previousApplications.indexOf(aName);
            int bIndex = previousApplications.indexOf(bName);
            if(aIndex > bIndex){
                return true;
            }
            if(aIndex < bIndex){
                return false;
            }
            return aName > bIndex;
        });
        return applications;
    }
    Q_INVOKABLE void breadcrumb(QString category, QString message, QString type = "default"){
#ifdef SENTRY
        sentry_breadcrumb(category.toStdString().c_str(), message.toStdString().c_str(), type.toStdString().c_str());
#else
        Q_UNUSED(category);
        Q_UNUSED(message);
        Q_UNUSED(type);
#endif
    }
    AppItem* getApplication(QString name){
        for(auto app : applications){
            if(app->property("name").toString() == name){
                return reinterpret_cast<AppItem*>(app);
            }
        }
        return nullptr;
    }
    QString state() {
        if(!getStateControllerUI()){
            return "loading";
        }
        return stateControllerUI->property("state").toString();
    }
    void setState(QString state){
        if(!getStateControllerUI()){
            throw "Unable to find state controller";
        }
        stateControllerUI->setProperty("state", state);
    }
    void saveScreen(){
        qDebug() << "Saving screen for background...";
        screenProvider->updateImage(EPFrameBuffer::framebuffer());
    }

    void setRoot(QObject* root){ this->root = root; }
    Apps* getAppsApi() { return appsApi; }

signals:
    void reload();

private slots:
    void sigUsr1(){
        ::kill(tarnishPid(), SIGUSR1);
        qDebug() << "Sent to the foreground...";
        saveScreen();
        setState("loading");
    }
    void sigUsr2(){
        qDebug() << "Sent to the background...";
        setState("hidden");
        QTimer::singleShot(0, [this]{
            ::kill(tarnishPid(), SIGUSR2);
        });
    }
    void unregisterApplication(QDBusObjectPath path){
        auto pathString = path.path();
        for(auto app : applications){
            if(app->property("path") == pathString){
                applications.removeAll(app);
                delete app;
                emit reload();
                qDebug() << "Removed" << pathString << "application";
                return;
            }
        }
        qDebug() << "Unable to find application " << pathString << "to remove";
    }
    void registerApplication(QDBusObjectPath path){
        qDebug() << "New application detected" << path.path();
        emit reload();
    }

private:
    QSettings settings;
    General* api;
    Screen* screenApi;
    Apps* appsApi;
    QObject* root = nullptr;
    QObject* stateControllerUI = nullptr;
    QObject* backgroundUI = nullptr;
    ScreenProvider* screenProvider;
    QList<QObject*> applications;

    int tarnishPid() { return api->tarnishPid(); }
    QObject* getStateControllerUI(){
        stateControllerUI = root->findChild<QObject*>("stateController");
        return stateControllerUI;
    }
    QObject* getBackgroundUI(){
        backgroundUI = root->findChild<QObject*>("background");
        return backgroundUI;
    }

    static void migrate(QSettings* settings, int fromVersion){
        if(fromVersion != 0){
            throw "Unknown settings version";
        }
        // In the future migrate changes to settings between versions
        settings->setValue("version", CORRUPT_SETTINGS_VERSION);
        settings->sync();
    }
};

#endif // CONTROLLER_H
