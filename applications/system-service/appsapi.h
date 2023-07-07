#ifndef APPSAPI_H
#define APPSAPI_H

#include <QDBusMetaType>
#include <QDebug>
#include <QDBusObjectPath>
#include <QSettings>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>

#include "apibase.h"
#include "application.h"

#define OXIDE_SETTINGS_VERSION 1

#define appsAPI AppsAPI::singleton()

using namespace Oxide;
using namespace Oxide::Applications;

class AppsAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPS_INTERFACE)
    Q_PROPERTY(int state READ state) // This needs to be here for the XML to generate the other properties :(
    Q_PROPERTY(QDBusObjectPath startupApplication READ startupApplication WRITE setStartupApplication)
    Q_PROPERTY(QDBusObjectPath lockscreenApplication READ lockscreenApplication WRITE setLockscreenApplication)
    Q_PROPERTY(QDBusObjectPath processManagerApplication READ processManagerApplication WRITE setProcessManagerApplication)
    Q_PROPERTY(QDBusObjectPath taskSwitcherApplication READ taskSwitcherApplication WRITE setTaskSwitcherApplication)
    Q_PROPERTY(QVariantMap applications READ getApplications)
    Q_PROPERTY(QStringList previousApplications READ getPreviousApplications)
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
        qDebug() << "Ensuring all applications have stopped...";
        while(!applications.isEmpty()){
            auto app = applications.take(applications.firstKey());
            delete app;
        }
    }
    void startup();
    void shutdown();
    int state() { return 0; } // Ignore this, it's a kludge to get the xml to generate

    void setEnabled(bool enabled){
        qDebug() << "Apps API" << enabled;
        m_enabled = enabled;
        for(auto app : applications){
            if(enabled){
                app->registerPath();
            }else{
                app->unregisterPath();
            }
        }
    }
    bool isEnabled(){ return m_enabled; }

    Q_INVOKABLE QDBusObjectPath registerApplication(QVariantMap properties){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        return registerApplicationNoSecurityCheck(properties);
    }
    QDBusObjectPath registerApplicationNoSecurityCheck(QVariantMap properties){
        QString name = properties.value("name", "").toString();
        QString bin = properties.value("bin", "").toString();
        int type = properties.value("type", ApplicationType::Foreground).toInt();
        if(type < ApplicationType::Foreground || type > ApplicationType::Backgroundable){
            qDebug() << "Invalid configuration: Invalid type" << type;
            return QDBusObjectPath("/");
        }
        if(name.isEmpty()){
            qDebug() << "Invalid configuration: Name is empty";
            return QDBusObjectPath("/");
        }
        if(bin.isEmpty() || !QFile::exists(bin)){
            qDebug() << "Invalid configuration: " << name << " has invalid bin" << bin;
            return QDBusObjectPath("/");
        }
        if(!QFileInfo(bin).isExecutable()){
            qDebug() << "Invalid configuration: " << name << " has bin that is not executable" << bin;
            return QDBusObjectPath("/");
        }
        if(applications.contains(name)){
            return applications[name]->qPath();
        }
        QDBusObjectPath path;
        Oxide::Sentry::sentry_transaction("apps", "registerApplication", [this, &path, name, properties](Oxide::Sentry::Transaction* t){
            Q_UNUSED(t);
            path = QDBusObjectPath(getPath(name));
            auto app = new Application(path, reinterpret_cast<QObject*>(this));
            app->moveToThread(this->thread());
            app->setConfig(properties);
            applications.insert(name, app);
            if(m_enabled){
                app->registerPath();
            }
            emit applicationRegistered(path);
        });
        return path;
    }
    Q_INVOKABLE bool unregisterApplication(QDBusObjectPath path){
        if(!hasPermission("apps")){
            return false;
        }
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

    Q_INVOKABLE void reload(){
        if(!hasPermission("apps")){
            return;
        }
        Oxide::Sentry::sentry_transaction("apps", "reload", [this](Oxide::Sentry::Transaction* t){
            Q_UNUSED(t);
            writeApplications();
            readApplications();
        });
    }

    QDBusObjectPath startupApplication(){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        return m_startupApplication;
    }
    void setStartupApplication(QDBusObjectPath path){
        if(!hasPermission("apps")){
            return;
        }
        if(getApplication(path) != nullptr){
            m_startupApplication = path;
            settings.setValue("startupApplication", path.path());
        }
    }
    QDBusObjectPath lockscreenApplication(){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        return m_lockscreenApplication;
    }
    void setLockscreenApplication(QDBusObjectPath path){
        if(!hasPermission("apps")){
            return;
        }
        if(getApplication(path) != nullptr){
            m_lockscreenApplication = path;
            settings.setValue("lockscreenApplication", path.path());
        }
    }
    QDBusObjectPath processManagerApplication(){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        return m_processManagerApplication;
    }
    void setProcessManagerApplication(QDBusObjectPath path){
        if(!hasPermission("apps")){
            return;
        }
        if(getApplication(path) != nullptr){
            m_processManagerApplication = path;
            settings.setValue("processManagerApplication", path.path());
        }
    }
    QDBusObjectPath taskSwitcherApplication(){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        return m_taskSwitcherApplication;
    }
    void setTaskSwitcherApplication(QDBusObjectPath path){
        if(!hasPermission("apps")){
            return;
        }
        if(getApplication(path) != nullptr){
            m_taskSwitcherApplication = path;
            settings.setValue("taskSwitcherApplication", path.path());
        }
    }

    QVariantMap getApplications(){
        QVariantMap result;
        if(!hasPermission("apps")){
            return result;
        }
        for(auto app : applications){
            result.insert(app->name(), QVariant::fromValue(app->qPath()));
        }
        return result;
    }

    QDBusObjectPath currentApplication(){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        return currentApplicationNoSecurityCheck();
    }
    QDBusObjectPath currentApplicationNoSecurityCheck(){
        for(auto app : applications){
            if(app->stateNoSecurityCheck() == Application::InForeground){
                return app->qPath();
            }
        }
        return QDBusObjectPath("/");
    }

    QVariantMap runningApplications(){
        if(!hasPermission("apps")){
            return QVariantMap();
        }
        return runningApplicationsNoSecurityCheck();
    }
    QVariantMap runningApplicationsNoSecurityCheck(){
        QVariantMap result;
        for(auto app : applications){
            auto state = app->stateNoSecurityCheck();
            if(state == Application::InForeground || state == Application::InBackground){
                result.insert(app->name(), QVariant::fromValue(app->qPath()));
            }
        }
        return result;
    }
    QVariantMap pausedApplications(){
        QVariantMap result;
        if(!hasPermission("apps")){
            return result;
        }
        for(auto app : applications){
            auto state = app->stateNoSecurityCheck();
            if(state == Application::Paused){
                result.insert(app->name(), QVariant::fromValue(app->qPath()));
            }
        }
        return result;
    }

    void unregisterApplication(Application* app){
        Oxide::Sentry::sentry_transaction("apps", "unregisterApplication", [this, app](Oxide::Sentry::Transaction* t){
            Q_UNUSED(t);
            auto name = app->name();
            if(applications.contains(name)){
                applications.remove(name);
                emit applicationUnregistered(app->qPath());
                app->deleteLater();
            }
        });
    }
    void pauseAll(){
        for(auto app : applications){
            app->pauseNoSecurityCheck(false);
        }
    }
    void resumeIfNone();
    Application* getApplication(QDBusObjectPath path){
        for(auto app : applications){
            if(app->path() == path.path()){
                return app;
            }
        }
        return nullptr;
    }
    QStringList getPreviousApplications(){ return previousApplications; }
    Q_INVOKABLE QDBusObjectPath getApplicationPath(const QString& name){
        if(!hasPermission("apps")){
            return QDBusObjectPath("/");
        }
        auto app = getApplication(name);
        if(app == nullptr){
            return QDBusObjectPath("/");
        }
        return app->qPath();
    }
    Application* getApplication(const QString& name){
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
    Q_INVOKABLE bool previousApplication(){
        if(!hasPermission("apps")){
            return false;
        }
        return previousApplicationNoSecurityCheck();
    }
    bool previousApplicationNoSecurityCheck(){
        if(locked() || m_stopping){
            return false;
        }
        if(previousApplications.isEmpty()){
            qDebug() << "No previous applications";
            return false;
        }
        bool found = false;
        while(!previousApplications.isEmpty()){
            auto name = previousApplications.takeLast();
            auto application = getApplication(name);
            if(application == nullptr){
                continue;
            }
            auto currentApplication = getApplication(this->currentApplicationNoSecurityCheck());
            if(currentApplication != nullptr){
                if(currentApplication == application){
                    continue;
                }
                currentApplication->pauseNoSecurityCheck(false);
            }
            application->launchNoSecurityCheck();
            qDebug() << "Resuming previous application" << application->name();
            found = true;
            break;
        }
        qDebug() << "Previous Applications" << previousApplications;
        return found;
    }
    void forceRecordPreviousApplication(){
        auto currentApplication = getApplication(this->currentApplicationNoSecurityCheck());
        if(currentApplication == nullptr){
            O_WARNING("Unable to find current application");
            return;
        }
        auto name = currentApplication->name();
        previousApplications.removeAll(name);
        previousApplications.append(name);
        qDebug() << "Previous Applications" << previousApplications;
    }
    void recordPreviousApplication(){
        auto currentApplication = getApplication(this->currentApplicationNoSecurityCheck());
        if(currentApplication == nullptr){
            O_WARNING("Unable to find current application");
            return;
        }
        if(currentApplication->qPath() == lockscreenApplication()){
            return;
        }
        if(currentApplication->qPath() == taskSwitcherApplication()){
            return;
        }
        auto name = currentApplication->name();
        removeFromPreviousApplications(name);
        previousApplications.append(name);
        qDebug() << "Previous Applications" << previousApplications;
    }
    void removeFromPreviousApplications(QString name){ previousApplications.removeAll(name); }
    bool stopping(){ return m_stopping; }

signals:
    void applicationRegistered(QDBusObjectPath);
    void applicationLaunched(QDBusObjectPath);
    void applicationUnregistered(QDBusObjectPath);
    void applicationPaused(QDBusObjectPath);
    void applicationResumed(QDBusObjectPath);    void applicationSignaled(QDBusObjectPath);
    void applicationExited(QDBusObjectPath, int);

public slots:
    QT_DEPRECATED void leftHeld(){ openDefaultApplication(); }
    void openDefaultApplication(){
        if(locked() || !hasPermission("apps") || m_stopping){
            return;
        }
        auto path = this->currentApplicationNoSecurityCheck();
        if(path.path() != "/"){
            auto currentApplication = getApplication(path);
            if(
                currentApplication != nullptr
                && currentApplication->stateNoSecurityCheck() != Application::Inactive
                && (path == m_startupApplication || path == m_lockscreenApplication)
            ){
                qDebug() << "Already in default application";
                return;
            }
        }
        auto app = getApplication(m_startupApplication);
        if(app == nullptr){
            qDebug() << "Unable to find default application";
            return;
        }
        qDebug() << "Opening default application";
        app->launchNoSecurityCheck();
    }
    QT_DEPRECATED void homeHeld(){ openTaskManager(); }
    void openTaskManager(){
        if(locked() || !hasPermission("apps") || m_stopping){
            return;
        }
        auto path = this->currentApplicationNoSecurityCheck();
        if(path.path() != "/"){
            auto currentApplication = getApplication(path);
            if(
                currentApplication != nullptr
                && currentApplication->stateNoSecurityCheck() != Application::Inactive
                && path == m_lockscreenApplication
            ){
                qDebug() << "Can't open task manager, on the lockscreen";
                return;
            }
        }
        auto app = getApplication(m_processManagerApplication);
        if(app == nullptr){
            qDebug() << "Unable to find task manager";
            return;
        }
        qDebug() << "Opening task manager";
        app->launchNoSecurityCheck();
    }
    void openLockScreen(){
        if(locked() || !hasPermission("apps") || m_stopping){
            return;
        }
        auto path = this->currentApplicationNoSecurityCheck();
        if(path.path() != "/"){
            auto currentApplication = getApplication(path);
            if(
                currentApplication != nullptr
                && currentApplication->stateNoSecurityCheck() != Application::Inactive
                && path == m_lockscreenApplication
            ){
                qDebug() << "Already on the lockscreen";
                return;
            }
        }
        auto app = getApplication(m_lockscreenApplication);
        if(app == nullptr){
            qDebug() << "Unable to find lockscreen";
            return;
        }
        qDebug() << "Opening lock screen";
        app->launchNoSecurityCheck();
    }
    void openTaskSwitcher(){
        if(locked() || !hasPermission("apps") || m_stopping){
            return;
        }
        auto path = this->currentApplicationNoSecurityCheck();
        if(path.path() != "/"){
            auto currentApplication = getApplication(path);
            if(
                currentApplication != nullptr
                && currentApplication->stateNoSecurityCheck() != Application::Inactive
            ){
                if(path == m_lockscreenApplication){
                    qDebug() << "Can't open task switcher, on the lockscreen";
                    return;
                }
                if(path == m_taskSwitcherApplication){
                    qDebug() << "Already on the task switcher";
                    return;
                }
            }
        }
        auto app = getApplication(m_taskSwitcherApplication);
        if(app != nullptr){
            app->launchNoSecurityCheck();
            return;
        }
        app = getApplication(m_startupApplication);
        if(app == nullptr){
            qDebug() << "Unable to find default application";
            return;
        }
        qDebug() << "Opening task switcher";
        app->launchNoSecurityCheck();
    }

private:
    bool m_stopping;
    bool m_starting;
    bool m_enabled;
    QMap<QString, Application*> applications;
    QStringList previousApplications;
    QSettings settings;
    QDBusObjectPath m_startupApplication;
    QDBusObjectPath m_lockscreenApplication;
    QDBusObjectPath m_processManagerApplication;
    QDBusObjectPath m_taskSwitcherApplication;
    bool m_sleeping;
    Application* resumeApp = nullptr;
    QString getPath(QString name){
        static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));
        return QString(OXIDE_SERVICE_PATH) + "/apps/" + QUuid::createUuidV5(NS, name).toString(QUuid::Id128);
    }
    QString _noApplicationsMessage = "No applications have been found. This is the result of invalid configuration. Open an issue on\nhttps://github.com/Eeems-Org/oxide\nto get support resolving this.";
    QString _noForegroundAppMessage = "No foreground application currently running. Open an issue on\nhttps://github.com/Eeems-Org/oxide\nto get support resolving this.";

    void writeApplications(){
        auto apps = applications.values();
        int size = apps.size();
        settings.beginWriteArray("applications", size);
        for(int i = 0; i < size; ++i){
            settings.setArrayIndex(i);
            auto app = apps[i];
            auto config = app->getConfig();
            // Add/Update config items
            for(auto key : config.keys()){
                settings.setValue(key, config[key]);
            }
            // Remove old config
            for(auto key : settings.childKeys()){
                if(!config.contains(key)){
                    settings.remove(key);
                }
            }
        }
        settings.endArray();
    }
    void readApplications(){
        settings.sync();
        if(!applications.empty()){
            // Unregister any applications that have been removed from the settings file
            int size = settings.beginReadArray("applications");
            QStringList names;
            for(int i = 0; i < size; ++i){
                settings.setArrayIndex(i);
                names << settings.value("name").toString();
            }
            settings.endArray();
            for(auto name : applications.keys()){
                auto app = applications[name];
                if(!names.contains(name) && !app->systemApp() && !app->transient()){
                    app->unregisterNoSecurityCheck();
                }
            }
        }
        // Register/Update applications from the settings file
        int size = settings.beginReadArray("applications");
        for(int i = 0; i < size; ++i){
            settings.setArrayIndex(i);
            auto name = settings.value("name").toString();
            auto displayName = settings.value("displayName", name).toString();
            auto type = settings.value("type", Foreground).toInt();
            auto bin = settings.value("bin").toString();
            if(type < Foreground || type > Backgroundable || name.isEmpty() || bin.isEmpty()){
#ifdef DEBUG
                qDebug() << "Invalid configuration " << name;
#endif
                continue;
            }
            QVariantMap properties {
                {"name", name},
                {"displayName", displayName},
                {"description", settings.value("description", displayName).toString()},
                {"bin", bin},
                {"type", type},
                {"flags", settings.value("flags", QStringList()).toStringList()},
                {"icon", settings.value("icon", "").toString()},
                {"onPause", settings.value("onPause", "").toString()},
                {"onResume", settings.value("onResume", "").toString()},
                {"onStop", settings.value("onStop", "").toString()},
                {"environment", settings.value("environment", QVariantMap()).toMap()},
                {"workingDirectory", settings.value("workingDirectory", "").toString()},
                {"directories", settings.value("directories", QStringList()).toStringList()},
                {"permissions", settings.value("permissions", QStringList()).toStringList()},
                {"splash", settings.value("splash", "").toString()},
            };
            if(settings.contains("user")){
                properties.insert("user", settings.value("user", "").toString());
            }
            if(settings.contains("group")){
                properties.insert("group", settings.value("group", "").toString());
            }
            if(applications.contains(name)){
#ifdef DEBUG
                qDebug() << "Updating " << name;
                qDebug() << properties;
#endif
                applications[name]->setConfig(properties);
            }else{
                qDebug() << name;
#ifdef DEBUG
                qDebug() << properties;
#endif
                registerApplicationNoSecurityCheck(properties);
            }
        }
        settings.endArray();
        // Load system applications from disk
        QDir dir(OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY);
        dir.setNameFilters(QStringList() << "*.oxide");
        QMap<QString, QJsonObject> apps;
        for(auto entry : dir.entryInfoList()){
            auto app = getRegistration(entry.filePath());
            if(app.isEmpty()){
                qDebug() << "Invalid file " << entry.filePath();
                continue;
            }
            auto name = entry.completeBaseName();
            app["name"] = name;
            apps.insert(name, app);
        }
        // Unregister any system applications that no longer exist on disk.
        for(auto application : applications.values()){
            auto name = application->name();
            if(!apps.contains(name) && application->systemApp()){
                qDebug() << name << "Is no longer found on disk";
                application->unregisterNoSecurityCheck();
            }
        }
        // Register/Update any system application.
        for(auto app : apps){
            auto name = app["name"].toString();
            auto bin = app["bin"].toString();
            if(bin.isEmpty() || !QFile::exists(bin)){
                qDebug() << name << "Can't find application binary:" << bin;
#ifdef DEBUG
                qDebug() << app;
#endif
                continue;
            }
            if(!app.contains("flags") || !app["flags"].isArray()){
                app["flags"] = QJsonArray();
            }
            auto flags = app["flags"].toArray();
            flags.prepend("system");
            app["flags"] = flags;
            auto properties = registrationToMap(app);
            if(applications.contains(name)){
#ifdef DEBUG
                qDebug() << "Updating " << name;
                qDebug() << properties;
#endif
                applications[name]->setConfig(properties);
            }else{
                qDebug() << "New system app" << name;
#ifdef DEBUG
                qDebug() << properties;
#endif
                registerApplicationNoSecurityCheck(properties);
            }
        }
    }
    static void migrate(QSettings* settings, int fromVersion){
        if(fromVersion != 0){
            throw "Unknown settings version";
        }
        // In the future migrate changes to settings between versions
        settings->setValue("version", OXIDE_SETTINGS_VERSION);
        settings->sync();
    }
    bool locked();
    void ensureForegroundApp();
};
#endif // APPSAPI_H
