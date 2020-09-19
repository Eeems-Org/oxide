#ifndef APPSAPI_H
#define APPSAPI_H

#include <QDBusMetaType>
#include <QDebug>
#include <QDBusObjectPath>
#include <QSettings>
#include <QUuid>
#include <QFile>

#include "apibase.h"
#include "application.h"
#include "fb2png.h"

#define PNG_PATH "/tmp/fb.png"

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
            auto displayName = settings.value("displayname", name).toString();
            auto app = new Application(getPath(name), this);
            app->load(
                name, displayName,
                settings.value("description", displayName).toString(),
                settings.value("bin").toString(),
                settings.value("type", Foreground).toInt(),
                settings.value("autostart", false).toBool(),
                settings.value("systemapp", false).toBool(),
                settings.value("icon", "").toString(),
                settings.value("onpause", "").toString(),
                settings.value("onresume", "").toString(),
                settings.value("onstop", "").toString()
            );
            applications.insert(name, app);
        }
        settings.endArray();
        if(!applications.size()){
            // TODO load from draft config files
        }
        if(!applications.contains("xochitl")){
            auto app = new Application(getPath("Xochitl"), this);
            app->load(
                "xochitl",
                "Xochitl",
                "reMarkable default application",
                "/usr/bin/xochitl",
                Foreground, false, true
            );
            applications[app->name()] = app;
            emit applicationRegistered(app->qPath());
        }
        if(!applications.contains("codes.eeems.erode")){
            auto app = new Application(getPath("Process Manager"), this);
            app->load(
                "codes.eeems.erode",
                "Process Manager",
                "List/kill processes",
                "/opt/bin/erode",
                Foreground, false, true
            );
            applications[app->name()] = app;
            emit applicationRegistered(app->qPath());
        }
        auto path = QDBusObjectPath(settings.value("startupApplication").toString());
        auto app = getApplication(path);
        if(app == nullptr){
            app = getApplication("codes.eeems.oxide");
            if(app == nullptr){
                app = new Application(getPath("Launcher"), this);
                app->load(
                    "codes.eeems.oxide",
                    "Launcher",
                    "Application launcher",
                    "/opt/bin/oxide",
                    Foreground, false, true
                );
                applications[app->name()] = app;
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
        QString bin = properties.value("bin", "").toString();
        int type = properties.value("type", Foreground).toInt();
        if(type < Foreground || type > Background || name.isEmpty() || bin.isEmpty()){
            return QDBusObjectPath("/");
        }
        if(applications.contains(name)){
            return applications[name]->qPath();
        }
        auto path = QDBusObjectPath(getPath(name));
        auto app = new Application(path, this);
        auto displayName = properties.value("displayName", name).toString();
        app->load(
            name,
            displayName,
            properties.value("description", displayName).toString(),
            bin,
            type,
            properties.value("autoStart", false).toBool(),
            false,
            properties.value("icon", "").toString(),
            properties.value("onPause", "").toString(),
            properties.value("onResume", "").toString(),
            properties.value("onStop", "").toString()
        );
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
    void rightHeld(){
        takeScreenshot();
        if(QFile("/tmp/.screenshot").exists()){
            // Then execute the contents of /tmp/.terminate
            qDebug() << "Screenshot file exists.";
            system("/bin/bash /tmp/.screenshot");
        }
        qDebug() << "Screenshot done.";
    }
    void powerHeld(){

    }

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
            settings.setValue("displayname", app->displayName());
            settings.setValue("description", app->description());
            settings.setValue("bin", app->bin());
            settings.setValue("type", app->type());
            settings.setValue("autostart", app->autoStart());
            settings.setValue("systemaapp", app->systemApp());
            settings.setValue("icon", app->icon());
            settings.setValue("onstop", app->onPause());
            settings.setValue("onresume", app->onResume());
            settings.setValue("onstop", app->onStop());
        }
        settings.endArray();
    }
    static void removeScreenshot(){
        QFile file(PNG_PATH);
        if(file.exists()){
            qDebug() << "Removing framebuffer image";
            file.remove();
        }
    }
    static void takeScreenshot(){
        qDebug() << "Taking screenshot";
        int res = fb2png_defaults();
        if(res){
            qDebug() << "Failed to take screenshot: " << res;
        }
    }
};
#endif // APPSAPI_H
