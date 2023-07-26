#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QImage>
#include <QQuickItem>
#include <QGuiApplication>
#include <QScreen>

#include <epframebuffer.h>
#include <signal.h>
#include <liboxide.h>
#include <liboxide/tarnish.h>

#include "screenprovider.h"
#include "appitem.h"

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide;

#define CORRUPT_SETTINGS_VERSION 1

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
enum WifiState { WifiUnknown, WifiOff, WifiDisconnected, WifiOffline, WifiOnline};

class Controller : public QObject {
    Q_OBJECT

public:
    Controller(QObject* parent, ScreenProvider* screenProvider)
    : QObject(parent),applications() {
        blankImage = new QImage(qApp->primaryScreen()->geometry().size(), QImage::Format_Mono);
        this->screenProvider = screenProvider;
        SignalHandler::setup_unix_signal_handlers();
        connect(signalHandler, &SignalHandler::sigCont, this, &Controller::sigUsr1);
        connect(signalHandler, &SignalHandler::sigUsr1, this, &Controller::sigUsr1);
        connect(signalHandler, &SignalHandler::sigUsr2, this, &Controller::sigUsr2);
        connect(signalHandler, &SignalHandler::sigTerm, qApp, &QCoreApplication::quit);
        connect(signalHandler, &SignalHandler::sigInt, qApp, &QCoreApplication::quit);
        connect(signalHandler, &SignalHandler::sigPipe, qApp, &QCoreApplication::quit);

        qDebug() << "Requesting screen API...";
        screenApi = Oxide::Tarnish::screenApi().get();
        if(screenApi == nullptr){
            qDebug() << "Unable to get screen API";
            throw "";
        }

        qDebug() << "Requesting apps API...";
        appsApi = Oxide::Tarnish::appsApi().get();
        if(appsApi == nullptr){
            qDebug() << "Unable to get apps API";
            throw "";
        }
        connect(appsApi, &Apps::applicationUnregistered, this, &Controller::unregisterApplication);
        connect(appsApi, &Apps::applicationRegistered, this, &Controller::registerApplication);
        connect(appsApi, &Apps::applicationLaunched, this, &Controller::reload);
        connect(appsApi, &Apps::applicationExited, this, &Controller::reload);

        updateImage();
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
                appItem->deleteLater();
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
    void updateImage(){
        qDebug() << "Updating background...";
        Oxide::Sentry::sentry_transaction("controller", "updateImage", [this](Oxide::Sentry::Transaction* t){
            QImage* img = nullptr;
            Oxide::Sentry::sentry_span(t, "previousApplications", "Get image from previous application", [this, &img](Oxide::Sentry::Span* s){
                auto previousApplications = appsApi->previousApplications();
                while(img == nullptr && !previousApplications.isEmpty()){
                    auto name = previousApplications.takeLast();
                    Oxide::Sentry::sentry_span(s, name.toStdString(), "Load image from application", [this, &img, previousApplications, name]{
                        auto path = ((QDBusObjectPath)appsApi->getApplicationPath(name)).path();
                        if(path == "/"){
                            O_WARNING("Unable to get save screen for" << name);
                            return;
                        }
                        auto bus = QDBusConnection::systemBus();
                        Application app(OXIDE_SERVICE, path, bus, this);
                        auto data = app.screenCapture();
                        auto image = QImage::fromData(data, "JPG");
                        if(image.isNull()){
                            O_WARNING("Image for " << name << " is corrupt, trying next application");
                            return;
                        }
                        img = new QImage(image);
                        qDebug() << "Using save screen from " << name;
                    });
                }
            });
            Oxide::Sentry::sentry_span(t, "update", "Update image", [this, img]{
                if(img != nullptr){
                    screenProvider->updateImage(img);
                    return;
                }
                qWarning() << "No previous application. Using blank screen";
                screenProvider->updateImage(blankImage);
            });
        });
    }

    void setRoot(QObject* root){ this->root = root; }
    Apps* getAppsApi() { return appsApi; }

signals:
    void reload();

private slots:
    void sigUsr1(){
        ::kill(tarnishPid(), SIGUSR1);
        qDebug() << "Sent to the foreground...";
        setState("loading");
        updateImage();
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
    Screen* screenApi;
    Apps* appsApi;
    QObject* root = nullptr;
    QObject* stateControllerUI = nullptr;
    ScreenProvider* screenProvider;
    QList<QObject*> applications;
    QImage* blankImage;

    int tarnishPid() { return Oxide::Tarnish::tarnishPid(); }
    QObject* getStateControllerUI(){
        stateControllerUI = root->findChild<QObject*>("stateController");
        return stateControllerUI;
    }
};

#endif // CONTROLLER_H
