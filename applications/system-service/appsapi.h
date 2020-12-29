#ifndef APPSAPI_H
#define APPSAPI_H

#include <QDBusMetaType>
#include <QDebug>
#include <QDBusObjectPath>
#include <QSettings>
#include <QUuid>
#include <QFile>
#include <QTimer>

#include "apibase.h"
#include "application.h"
#include "signalhandler.h"

#define OXIDE_SETTINGS_VERSION 1

#define appsAPI AppsAPI::singleton()

class AppsAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPS_INTERFACE)
    Q_PROPERTY(int state READ state) // This needs to be here for the XML to generate the other properties :(
    Q_PROPERTY(QDBusObjectPath startupApplication READ startupApplication WRITE setStartupApplication)
    Q_PROPERTY(QVariantMap applications READ getApplications)
    Q_PROPERTY(QDBusObjectPath currentApplication READ currentApplication)
    Q_PROPERTY(QVariantMap runningApplications READ runningApplications)
    Q_PROPERTY(QVariantMap pausedApplications READ pausedApplications)
public:
    static AppsAPI* singleton(AppsAPI* self = nullptr){
        static AppsAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    AppsAPI(QObject* parent);
    ~AppsAPI() {
        m_stopping = true;
        writeApplications();
        settings.sync();
        for(auto app : applications){
            app->stop();
        }
        for(auto app : applications){
            app->waitForFinished();
            app->deleteLater();
        }
        applications.clear();
    }
    void startup();
    int state() { return 0; } // Ignore this, it's a kludge to get the xml to generate

    enum ApplicationType { Foreground, Background, Backgroundable};
    Q_ENUM(ApplicationType)

    void setEnabled(bool enabled){
        qDebug() << "Apps API" << enabled;
        for(auto app : applications){
            if(enabled){
                app->registerPath();
            }else{
                app->unregisterPath();
            }
        }
    }

    Q_INVOKABLE QDBusObjectPath registerApplication(QVariantMap properties){
        QString name = properties.value("name", "").toString();
        QString bin = properties.value("bin", "").toString();
        int type = properties.value("type", Foreground).toInt();
        if(type < Foreground || type > Background || name.isEmpty() || bin.isEmpty()){
            return QDBusObjectPath("/");
        }
        if(applications.contains(name)){
            return applications[name]->qPath();
        }
        auto path = QDBusObjectPath(getPath(name));
        auto app = new Application(path, reinterpret_cast<QObject*>(this));
        auto displayName = properties.value("displayName", name).toString();
        app->setConfig(properties);
        applications.insert(name, app);
        writeApplications();
        app->registerPath();
        emit applicationRegistered(path);
        return path;
    }
    Q_INVOKABLE bool unregisterApplication(QDBusObjectPath path){
        auto app = getApplication(path);
        if(app == nullptr){
            return true;
        }
        if(app->systemApp()){
            return false;
        }
        unregisterApplication(app);
        return true;
    }

    QDBusObjectPath startupApplication(){
        return m_startupApplication;
    }
    void setStartupApplication(QDBusObjectPath path){
        if(getApplication(path) != nullptr){
            m_startupApplication = path;
            settings.setValue("startupApplication", path.path());
        }
    }

    QVariantMap getApplications(){
        QVariantMap result;
        for(auto app : applications){
            result.insert(app->name(), QVariant::fromValue(app->qPath()));
        }
        return result;
    }

    QDBusObjectPath currentApplication(){
        for(auto app : applications){
            if(app->state() == Application::InForeground){
                return app->qPath();
            }
        }
        return QDBusObjectPath("/");
    }
    QVariantMap runningApplications(){
        QVariantMap result;
        for(auto app : applications){
            auto state = app->state();
            if(state == Application::InForeground || state == Application::InBackground){
                result.insert(app->name(), QVariant::fromValue(app->qPath()));
            }
        }
        return result;
    }
    QVariantMap pausedApplications(){
        QVariantMap result;
        for(auto app : applications){
            auto state = app->state();
            if(state == Application::Paused){
                result.insert(app->name(), QVariant::fromValue(app->qPath()));
            }
        }
        return result;
    }

    void unregisterApplication(Application* app){
        auto name = app->name();
        if(applications.contains(name)){
            applications.remove(name);
            emit applicationUnregistered(app->qPath());
            app->deleteLater();
            writeApplications();
        }
    }
    void pauseAll(){
        for(auto app : applications){
            app->pause(false);
        }
    }
    void resumeIfNone(){
        if(m_stopping){
            return;
        }
        for(auto app : applications){
            if(app->state() == Application::InForeground){
                return;
            }
        }
        getApplication(m_startupApplication)->launch();
    }
    Application* getApplication(QDBusObjectPath path){
        for(auto app : applications){
            if(app->path() == path.path()){
                return app;
            }
        }
        return nullptr;
    }
    Q_INVOKABLE QDBusObjectPath getApplicationPath(QString name){
        auto app = getApplication(name);
        if(app == nullptr){
            return QDBusObjectPath("/");
        }
        return app->qPath();
    }
    Application* getApplication(QString name){
        if(applications.contains(name)){
            return applications[name];
        }
        return nullptr;
    }
    void connectSignals(Application* app, int signal){
        switch(signal){
            case 1:
                connect(signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1);
            break;
            case 2:
                connect(signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2);
            break;
        }
    }
    void disconnectSignals(Application* app, int signal){
        switch(signal){
            case 1:
                disconnect(signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1);
            break;
            case 2:
                disconnect(signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2);
            break;
        }
    }
signals:
    void applicationRegistered(QDBusObjectPath);
    void applicationLaunched(QDBusObjectPath);
    void applicationUnregistered(QDBusObjectPath);
    void applicationPaused(QDBusObjectPath);
    void applicationResumed(QDBusObjectPath);
    void applicationSignaled(QDBusObjectPath);
    void applicationExited(QDBusObjectPath, int);

public slots:
    void leftHeld(){
        auto currentApplication = getApplication(this->currentApplication());
        if(currentApplication->state() != Application::Inactive && currentApplication->path() == m_startupApplication.path()){
            qDebug() << "Already at startup application";
            return;
        }
        getApplication(m_startupApplication)->launch();
    }
    void homeHeld(){
        auto app = getApplication("codes.eeems.erode");
        if(app == nullptr){
            qDebug() << "Unable to find process manager";
            return;
        }
        if(app->state() == Application::InForeground){
            qDebug() << "Process manager already running";
            return;
        }
        app->launch();
    }

private:
    bool m_stopping;
    bool m_enabled;
    QMap<QString, Application*> applications;
    QSettings settings;
    QDBusObjectPath m_startupApplication;
    bool m_sleeping;
    Application* resumeApp = nullptr;
    QString getPath(QString name){
        static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));
        return QString(OXIDE_SERVICE_PATH) + "/apps/" + QUuid::createUuidV5(NS, name).toString(QUuid::Id128);
    }
    void writeApplications(){
        auto apps = applications.values();
        int size = apps.size();
        settings.beginWriteArray("applications", size);
        for(int i = 0; i < size; ++i){
            settings.setArrayIndex(i);
            auto app = apps[i];
            auto config = app->getConfig();
            for(auto key : config.keys()){
                settings.setValue(key, config[key]);
            }
        }
        settings.endArray();
    }
    static void migrate(QSettings* settings, int fromVersion){
        Q_UNUSED(settings)
        Q_UNUSED(fromVersion)
        // In the future migrate changes to settings between versions
    }
};
#endif // APPSAPI_H
