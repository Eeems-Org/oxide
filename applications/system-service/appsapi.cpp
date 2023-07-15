#include <liboxide.h>

#include <QDBusMetaType>
#include <QDebug>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "appsapi.h"
#include "guiapi.h"
#include "notificationapi.h"

using namespace Oxide;

AppsAPI* AppsAPI::singleton(AppsAPI* self){
    static AppsAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

AppsAPI::AppsAPI(QObject* parent)
    : APIBase(parent),
      m_stopping(false),
      m_starting(true),
      m_enabled(false),
      applications(),
      previousApplications(),
      settings(this),
      m_startupApplication("/"),
  m_lockscreenApplication("/"),
  m_processManagerApplication("/"),
  m_taskSwitcherApplication("/"),
  m_sleeping(false) {
    Oxide::Sentry::sentry_transaction("apps", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "start", "Launching initial application", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "singleton", "Setup singleton", [this]{
                singleton(this);
            });
            qDBusRegisterMetaType<QMap<QString,QDBusObjectPath>>();
            qDBusRegisterMetaType<QDBusObjectPath>();
            Oxide::Sentry::sentry_span(s, "sync", "Sync settings", [this]{
                settings.sync();
            });
            auto version = settings.value("version", 0).toInt();
            if(version < OXIDE_SETTINGS_VERSION){
                Oxide::Sentry::sentry_span(s, "migrate", "Migrate to latest version", [this, version]{
                    migrate(&settings, version);
                });
            }
        });
        Oxide::Sentry::sentry_span(t, "application", "Read applications from disk", [this]{
            readApplications();
        });
        Oxide::Sentry::sentry_span(t, "setup", "Setup lockscreen, startup, process manager, task switcher", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "lockscreenApplication", "Determine what the lockscreen application is", [this]{
                auto path = QDBusObjectPath(settings.value("lockscreenApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    app = getApplication("codes.eeems.decay");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_lockscreenApplication = path;
            });
            Oxide::Sentry::sentry_span(s, "startupApplication", "Determine what the startup application is", [this]{
                auto path = QDBusObjectPath(settings.value("startupApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    app = getApplication("codes.eeems.oxide");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_startupApplication = path;
            });
            Oxide::Sentry::sentry_span(s, "processManagerApplication", "Determine what the process manager application is", [this]{
                auto path = QDBusObjectPath(settings.value("processManagerApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    app = getApplication("codes.eeems.erode");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_processManagerApplication= path;
            });
            Oxide::Sentry::sentry_span(s, "taskSwitcherApplication", "Determine what the task switcher application is", [this]{
                auto path = QDBusObjectPath(settings.value("taskSwitcherApplication").toString());
                auto app = getApplication(path);
                if(app == nullptr){
                    path = QDBusObjectPath(settings.value("processManagerApplication").toString());
                    app = getApplication(path);
                }
                if(app == nullptr){
                    app = getApplication("codes.eeems.corrupt");
                    if(app != nullptr){
                        path = app->qPath();
                    }
                }
                m_taskSwitcherApplication= path;
            });
        });
    });
}

AppsAPI::~AppsAPI() {
    m_stopping = true;
    writeApplications();
    settings.sync();
    qDebug() << "Ensuring all applications have stopped...";
    while(!applications.isEmpty()){
        auto app = applications.take(applications.firstKey());
        delete app;
    }
}

void AppsAPI::startup(){
    O_DEBUG(__PRETTY_FUNCTION__ << "Startup")
    Application::m_window = guiAPI->_createWindow(deviceSettings.screenGeometry(), DEFAULT_IMAGE_FORMAT);
    Oxide::Sentry::sentry_transaction("apps", "startup", [this](Oxide::Sentry::Transaction* t){
        if(applications.isEmpty()){
            qDebug() << "No applications found";
            notificationAPI->errorNotification(_noApplicationsMessage);
            return;
        }
        Oxide::Sentry::sentry_span(t, "autoStart", "Launching auto start applications", [this](Oxide::Sentry::Span* s){
            for(auto app : applications){
                if(app->autoStart()){
                    qDebug() << "Auto starting" << app->name();
                    Oxide::Sentry::sentry_span(s, app->name().toStdString(), "Launching application", [app]{
                        app->launchNoSecurityCheck();
                        if(app->type() == Backgroundable){
                            qDebug() << "  Pausing auto started app" << app->name();
                            app->pauseNoSecurityCheck();
                        }
                    });
                }
            }
        });
        Oxide::Sentry::sentry_span(t, "start", "Launching initial application", [this]{
            auto app = getApplication(m_lockscreenApplication);
            if(app == nullptr){
                qDebug() << "Could not find lockscreen application";
                app = getApplication(m_startupApplication);
            }
            if(app == nullptr){
                qDebug() << "Could not find startup application";
                qDebug() << "Using xochitl due to invalid configuration";
                app = getApplication("xochitl");
            }
            if(app == nullptr){
                qDebug() << "Could not find xochitl";
                qWarning() << "Using the first application in the list due to invalid configuration";
                app = applications.first();
            }
            qDebug() << "Starting initial application" << app->name();
            app->launchNoSecurityCheck();
            ensureForegroundApp();
        });
    });
}

void AppsAPI::shutdown(){
    m_stopping = true;
    auto frameBuffer = EPFrameBuffer::framebuffer();
    auto rect = frameBuffer->rect();
    QPainter painter(frameBuffer);
    auto fm = painter.fontMetrics();
    auto size = frameBuffer->size();
    qDebug() << "Clearing screen...";
    painter.setPen(Qt::white);
    painter.fillRect(rect, Qt::black);
    painter.end();
    EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::FullUpdate, true);
    qDebug() << "Stopping applications...";
    for(auto app : applications){
        if(app->stateNoSecurityCheck() != Application::Inactive){
            painter.begin(frameBuffer);
            painter.setPen(Qt::white);
            painter.fillRect(rect, Qt::black);
            auto text = "Stopping " + app->displayName() + "...";
            qDebug() << text.toStdString().c_str();
            int padding = 10;
            int textHeight = fm.height() + padding;
            QRect textRect(
                QPoint(0 + padding, size.height() - textHeight),
                QSize(size.width() - padding * 2, textHeight)
            );
            painter.fillRect(textRect, Qt::black);
            painter.drawText(
                textRect,
                Qt::AlignVCenter | Qt::AlignRight,
                text
            );
            EPFrameBuffer::sendUpdate(textRect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
            EPFrameBuffer::waitForLastUpdate();
            painter.end();
        }
        app->stopNoSecurityCheck();
    }
    qDebug() << "Ensuring all applications have stopped...";
    for(auto app : applications){
        app->waitForFinished();
    }
    applications.clear();
    qDebug() << "Displaying final quit message...";
    painter.begin(frameBuffer);
    painter.setPen(Qt::white);
    painter.fillRect(rect, Qt::black);
    painter.fillRect(rect, Qt::black);
    painter.drawText(rect, Qt::AlignCenter,"Goodbye!");
    painter.end();
    EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::FullUpdate, true);
    EPFrameBuffer::waitForLastUpdate();
}

void AppsAPI::setEnabled(bool enabled){
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

bool AppsAPI::isEnabled(){ return m_enabled; }

QDBusObjectPath AppsAPI::registerApplication(QVariantMap properties){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    return registerApplicationNoSecurityCheck(properties);
}

QDBusObjectPath AppsAPI::registerApplicationNoSecurityCheck(QVariantMap properties){
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

bool AppsAPI::unregisterApplication(QDBusObjectPath path){
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

void AppsAPI::reload(){
    if(!hasPermission("apps")){
        return;
    }
    Oxide::Sentry::sentry_transaction("apps", "reload", [this](Oxide::Sentry::Transaction* t){
        Q_UNUSED(t);
        writeApplications();
        readApplications();
    });
}

QDBusObjectPath AppsAPI::startupApplication(){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    return m_startupApplication;
}

void AppsAPI::setStartupApplication(QDBusObjectPath path){
    if(!hasPermission("apps")){
        return;
    }
    if(getApplication(path) != nullptr){
        m_startupApplication = path;
        settings.setValue("startupApplication", path.path());
    }
}

QDBusObjectPath AppsAPI::lockscreenApplication(){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    return m_lockscreenApplication;
}

void AppsAPI::setLockscreenApplication(QDBusObjectPath path){
    if(!hasPermission("apps")){
        return;
    }
    if(getApplication(path) != nullptr){
        m_lockscreenApplication = path;
        settings.setValue("lockscreenApplication", path.path());
    }
}

QDBusObjectPath AppsAPI::processManagerApplication(){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    return m_processManagerApplication;
}

void AppsAPI::setProcessManagerApplication(QDBusObjectPath path){
    if(!hasPermission("apps")){
        return;
    }
    if(getApplication(path) != nullptr){
        m_processManagerApplication = path;
        settings.setValue("processManagerApplication", path.path());
    }
}

QDBusObjectPath AppsAPI::taskSwitcherApplication(){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    return m_taskSwitcherApplication;
}

void AppsAPI::setTaskSwitcherApplication(QDBusObjectPath path){
    if(!hasPermission("apps")){
        return;
    }
    if(getApplication(path) != nullptr){
        m_taskSwitcherApplication = path;
        settings.setValue("taskSwitcherApplication", path.path());
    }
}

QVariantMap AppsAPI::getApplications(){
    QVariantMap result;
    if(!hasPermission("apps")){
        return result;
    }
    for(auto app : applications){
        result.insert(app->name(), QVariant::fromValue(app->qPath()));
    }
    return result;
}

QDBusObjectPath AppsAPI::currentApplication(){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    return currentApplicationNoSecurityCheck();
}

QDBusObjectPath AppsAPI::currentApplicationNoSecurityCheck(){
    for(auto app : applications){
        if(app->stateNoSecurityCheck() == Application::InForeground){
            return app->qPath();
        }
    }
    return QDBusObjectPath("/");
}

QVariantMap AppsAPI::runningApplications(){
    if(!hasPermission("apps")){
        return QVariantMap();
    }
    return runningApplicationsNoSecurityCheck();
}

QVariantMap AppsAPI::runningApplicationsNoSecurityCheck(){
    QVariantMap result;
    for(auto app : applications){
        auto state = app->stateNoSecurityCheck();
        if(state == Application::InForeground || state == Application::InBackground){
            result.insert(app->name(), QVariant::fromValue(app->qPath()));
        }
    }
    return result;
}

QVariantMap AppsAPI::pausedApplications(){
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

void AppsAPI::unregisterApplication(Application* app){
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

void AppsAPI::pauseAll(){
    for(auto app : applications){
        app->pauseNoSecurityCheck(false);
    }
}

void AppsAPI::resumeIfNone(){
    if(m_stopping || m_starting){
        return;
    }
    for(auto app : applications){
        if(app->stateNoSecurityCheck() == Application::InForeground){
            return;
        }
    }
    if(previousApplicationNoSecurityCheck()){
        return;
    }
    auto app = getApplication(m_startupApplication);
    if(app == nullptr){
        app = getApplication("xochitl");
    }
    if(app == nullptr){
        if(applications.isEmpty()){
            qDebug() << "No applications found";
            notificationAPI->errorNotification(_noApplicationsMessage);
            return;
        }
        app = applications.first();
    }
    app->launchNoSecurityCheck();
    m_starting = true;
    ensureForegroundApp();
}

Application* AppsAPI::getApplication(QDBusObjectPath path){
    for(auto app : applications){
        if(app->path() == path.path()){
            return app;
        }
    }
    return nullptr;
}

QStringList AppsAPI::getPreviousApplications(){ return previousApplications; }

QDBusObjectPath AppsAPI::getApplicationPath(const QString& name){
    if(!hasPermission("apps")){
        return QDBusObjectPath("/");
    }
    auto app = getApplication(name);
    if(app == nullptr){
        return QDBusObjectPath("/");
    }
    return app->qPath();
}

Application* AppsAPI::getApplication(const QString& name){
    if(applications.contains(name)){
        return applications[name];
    }
    return nullptr;
}

void AppsAPI::connectSignals(Application* app, int signal){
    switch(signal){
        case 1:
            connect(signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1);
            break;
        case 2:
            connect(signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2);
            break;
    }
}

void AppsAPI::disconnectSignals(Application* app, int signal){
    switch(signal){
        case 1:
            disconnect(signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1);
            break;
        case 2:
            disconnect(signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2);
            break;
    }
}

bool AppsAPI::previousApplication(){
    if(!hasPermission("apps")){
        return false;
    }
    return previousApplicationNoSecurityCheck();
}

bool AppsAPI::previousApplicationNoSecurityCheck(){
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

void AppsAPI::forceRecordPreviousApplication(){
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

void AppsAPI::recordPreviousApplication(){
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

void AppsAPI::removeFromPreviousApplications(QString name){ previousApplications.removeAll(name); }

bool AppsAPI::stopping(){ return m_stopping; }

void AppsAPI::openDefaultApplication(){
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

void AppsAPI::openTaskManager(){
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

void AppsAPI::openLockScreen(){
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

void AppsAPI::openTaskSwitcher(){
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

QString AppsAPI::getPath(QString name){
    static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));
    return QString(OXIDE_SERVICE_PATH) + "/apps/" + QUuid::createUuidV5(NS, name).toString(QUuid::Id128);
}

void AppsAPI::writeApplications(){
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

void AppsAPI::readApplications(){
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

void AppsAPI::migrate(QSettings* settings, int fromVersion){
    if(fromVersion != 0){
        throw "Unknown settings version";
    }
    // In the future migrate changes to settings between versions
    settings->setValue("version", OXIDE_SETTINGS_VERSION);
    settings->sync();
}

bool AppsAPI::locked(){ return notificationAPI->locked(); }

void AppsAPI::ensureForegroundApp() {
    QTimer::singleShot(300, [this]{
        m_starting = false;
        auto path = appsAPI->currentApplicationNoSecurityCheck();
        if(path.path() == "/"){
            notificationAPI->errorNotification(_noForegroundAppMessage);
            return;
        }
        auto app = appsAPI->getApplication(path);
        if(app == nullptr || app->state() == Application::Inactive){
            notificationAPI->errorNotification(_noForegroundAppMessage);
            return;
        }
    });
}
