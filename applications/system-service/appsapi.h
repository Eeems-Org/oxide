#ifndef APPSAPI_H
#define APPSAPI_H

#include <QDBusMetaType>
#include <QDebug>
#include <QDBusObjectPath>
#include <QSettings>
#include <QUuid>

#include "apibase.h"
#include "application.h"

class AppsAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPS_INTERFACE)
    Q_PROPERTY(int state READ state) // This needs to be here for the XML to generate the other properties :(
    Q_PROPERTY(QDBusObjectPath startupApplication READ startupApplication WRITE setStartupApplication)
    Q_PROPERTY(QVariantMap applications READ getApplications)
    Q_PROPERTY(QDBusObjectPath currentApplication READ currentApplication)
    Q_PROPERTY(QVariantMap runningApplications READ runningApplications)
public:
    AppsAPI(QObject* parent)
    : APIBase(parent),
      m_enabled(false),
      applications(),
      settings(this),
      m_startupApplication("/") {
        qDBusRegisterMetaType<QMap<QString,QDBusObjectPath>>();
        qDBusRegisterMetaType<QDBusObjectPath>();
        settings.sync();
        int size = settings.beginReadArray("applications");
        for(int i = 0; i < size; ++i){
            settings.setArrayIndex(i);
            auto name = settings.value("name").toString();
            auto app = new Application(getPath(name), this);
            app->load(
                name,
                settings.value("description", "").toString(),
                settings.value("call").toString(),
                settings.value("term", "").toString(),
                settings.value("type", Foreground).toInt(),
                settings.value("autostart", false).toBool()
            );
            applications.insert(name, app);
        }
        settings.endArray();
        if(!applications.size()){
            // TODO load from draft config files
        }
        auto path = QDBusObjectPath(settings.value("startupApplication").toString());
        auto app = getApplication(path);
        if(app == nullptr){
            if(applications.contains("oxide")){
                app = applications["oxide"];
            }else{
                app = new Application(getPath("oxide"), this);
                app->load("oxide", "Application launcher", "/opt/bin/oxide", "", Foreground, false);
                applications["oxide"] = app;
                emit applicationRegistered(app->qPath());
            }
            path = app->qPath();
        }
        m_startupApplication = path;
        app->launch();
    }
    ~AppsAPI() {
        writeApplications();
        settings.sync();
        for(auto app : applications){
            switch(app->state()){
                case Application::Paused:
                    app->signal(SIGCONT);
                case Application::InForeground:
                case Application::InBackground:
                    app->signal(SIGTERM);
                break;
            }
        }
        for(auto app : applications){
            app->waitForFinished();
            delete app;
        }
        applications.clear();
    }
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
        QString call = properties.value("call", "").toString();
        int type = properties.value("type", Foreground).toInt();
        if(type < Foreground || type > Background || name.isEmpty() || call.isEmpty()){
            return QDBusObjectPath("/");
        }
        if(applications.contains(name)){
            return applications[name]->qPath();
        }
        auto path = QDBusObjectPath(getPath(name));
        auto app = new Application(path, this);
        app->load(
            name,
            properties.value("description", "").toString(),
            call,
            properties.value("term", "").toString(),
            type,
            properties.value("autostart", false).toBool()
        );
        applications.insert(name, app);
        if(m_enabled){
            app->registerPath();
        }
        writeApplications();
        emit applicationRegistered(path);
        return path;
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

    void unregisterApplication(Application* app){
        auto name = app->name();
        if(applications.contains(name)){
            applications.remove(name);
            emit applicationUnregistered(app->qPath());
            delete app;
            writeApplications();
        }
    }
    void pauseAll(){
        for(auto app : applications){
            app->pause(false);
        }
    }
    void resumeIfNone(){
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
signals:
    void applicationRegistered(QDBusObjectPath);
    void applicationLaunched(QDBusObjectPath);
    void applicationUnregistered(QDBusObjectPath);
    void applicationPaused(QDBusObjectPath);
    void applicationResumed(QDBusObjectPath);
    void applicationSignaled(QDBusObjectPath);
    void applicationExited(QDBusObjectPath, int);

private:
    bool m_enabled;
    QMap<QString, Application*> applications;
    QSettings settings;
    QDBusObjectPath m_startupApplication;
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
            settings.setValue("name", app->name());
            settings.setValue("description", app->description());
            settings.setValue("call", app->call());
            settings.setValue("term", app->term());
            settings.setValue("type", app->type());
            settings.setValue("autostart", app->autoStart());
        }
        settings.endArray();
    }
};
#endif // APPSAPI_H
