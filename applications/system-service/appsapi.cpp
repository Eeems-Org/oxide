#include <liboxide.h>

#include "appsapi.h"
#include "notificationapi.h"
#include "systemapi.h"

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
            Oxide::Sentry::sentry_span(s, "signals", "Setup unix signal handlers", []{
                SignalHandler::setup_unix_signal_handlers();
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

void AppsAPI::startup(){
    Oxide::Sentry::sentry_transaction("apps", "startup", [this](Oxide::Sentry::Transaction* t){
        if(applications.isEmpty()){
            O_INFO("No applications found");
            notificationAPI->errorNotification(_noApplicationsMessage);
            return;
        }
        Oxide::Sentry::sentry_span(t, "autoStart", "Launching auto start applications", [this](Oxide::Sentry::Span* s){
            for(auto app : applications){
                if(app->autoStart()){
                    O_INFO("Auto starting" << app->name());
                    Oxide::Sentry::sentry_span(s, app->name().toStdString(), "Launching application", [app]{
                        app->launchNoSecurityCheck();
                        if(app->type() == Backgroundable){
                            O_INFO("  Pausing auto started app" << app->name());
                            app->pauseNoSecurityCheck();
                        }
                    });
                }
            }
        });
        Oxide::Sentry::sentry_span(t, "start", "Launching initial application", [this]{
            auto app = getApplication(m_lockscreenApplication);
            if(app == nullptr){
                O_WARNING("Could not find lockscreen application");
                app = getApplication(m_startupApplication);
            }
            if(app == nullptr){
                O_WARNING("Could not find startup application");
                O_WARNING("Using xochitl due to invalid configuration");
                app = getApplication("xochitl");
            }
            if(app == nullptr){
                O_WARNING("Could not find xochitl");
                O_WARNING("Using the first application in the list due to invalid configuration");
                app = applications.first();
            }
            O_INFO("Starting initial application" << app->name());
            app->launchNoSecurityCheck();
            ensureForegroundApp();
        });
    });
}

void AppsAPI::setEnabled(bool enabled){
    O_INFO("Apps API" << enabled);
    for(auto app : applications){
        if(enabled){
            app->registerPath();
        }else{
            app->unregisterPath();
        }
    }
}

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
        O_WARNING("Invalid configuration: Invalid type" << type);
        return QDBusObjectPath("/");
    }
    if(name.isEmpty()){
        O_WARNING("Invalid configuration: Name is empty");
        return QDBusObjectPath("/");
    }
    if(bin.isEmpty() || !QFile::exists(bin)){
        O_WARNING("Invalid configuration: " << name << " has invalid bin" << bin);
        return QDBusObjectPath("/");
    }
    if(!QFileInfo(bin).isExecutable()){
        O_WARNING("Invalid configuration: " << name << " has bin that is not executable" << bin);
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
        auto displayName = properties.value("displayName", name).toString();
        app->setConfig(properties);
        applications.insert(name, app);
        app->registerPath();
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
            O_WARNING("No applications found");
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
    if(locked()){
        return false;
    }
    if(previousApplications.isEmpty()){
        O_DEBUG("No previous applications");
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
        O_INFO("Resuming previous application" << application->name());
        found = true;
        break;
    }
    O_DEBUG("Previous Applications" << previousApplications);
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
    O_DEBUG("Previous Applications" << previousApplications);
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
    O_DEBUG("Previous Applications" << previousApplications);
}

void AppsAPI::removeFromPreviousApplications(QString name){ previousApplications.removeAll(name); }

void AppsAPI::leftHeld(){ openDefaultApplication(); }

void AppsAPI::openDefaultApplication(){
    if(locked() || !hasPermission("apps")){
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
            O_DEBUG("Already in default application");
            return;
        }
    }
    auto app = getApplication(m_startupApplication);
    if(app == nullptr){
        O_WARNING("Unable to find default application");
        return;
    }
    O_INFO("Opening default application");
    app->launchNoSecurityCheck();
}

void AppsAPI::homeHeld(){ openTaskManager(); }

void AppsAPI::openTaskManager(){
    if(locked() || !hasPermission("apps")){
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
            O_WARNING("Can't open task manager, on the lockscreen");
            return;
        }
    }
    auto app = getApplication(m_processManagerApplication);
    if(app == nullptr){
        O_WARNING("Unable to find task manager");
        return;
    }
    O_INFO("Opening task manager");
    app->launchNoSecurityCheck();
}

void AppsAPI::openLockScreen(){
    if(locked() || !hasPermission("apps")){
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
            O_DEBUG("Already on the lockscreen");
            return;
        }
    }
    auto app = getApplication(m_lockscreenApplication);
    if(app == nullptr){
        O_WARNING("Unable to find lockscreen");
        return;
    }
    O_INFO("Opening lock screen");
    app->launchNoSecurityCheck();
}

void AppsAPI::openTaskSwitcher(){
    if(locked() || !hasPermission("apps")){
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
                O_WARNING("Can't open task switcher, on the lockscreen");
                return;
            }
            if(path == m_taskSwitcherApplication){
                O_WARNING("Already on the task switcher");
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
        O_WARNING("Unable to find default application");
        return;
    }
    O_INFO("Opening task switcher");
    app->launchNoSecurityCheck();
}

void AppsAPI::openTerminal(){
    if(locked() || !hasPermission("apps")){
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
                O_WARNING("Can't open task switcher, on the lockscreen");
                return;
            }
        }
    }
    auto app = getApplication("yaft");
    if(app != nullptr){
        app->launchNoSecurityCheck();
        return;
    }
    app = getApplication("fingerterm");
    if(app == nullptr){
        O_WARNING("Unable to find terminal application");
        return;
    }
    O_INFO("Opening terminal");
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
            O_DEBUG("Invalid configuration " << name);
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
            O_DEBUG("Updating " << name);
            O_DEBUG(properties);
            applications[name]->setConfig(properties);
        }else{
            O_INFO(name);
            O_DEBUG(properties);
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
            O_WARNING("Invalid file " << entry.filePath());
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
            O_WARNING(name << "Is no longer found on disk");
            application->unregisterNoSecurityCheck();
        }
    }
    // Register/Update any system application.
    for(auto app : apps){
        auto name = app["name"].toString();
        auto bin = app["bin"].toString();
        if(bin.isEmpty() || !QFile::exists(bin)){
            O_WARNING(name << "Can't find application binary:" << bin);
            O_DEBUG(app);
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
            O_DEBUG("Updating " << name);
            O_DEBUG(properties);
            applications[name]->setConfig(properties);
        }else{
            O_INFO("New system app" << name);
            O_DEBUG(properties);
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

AppsAPI::~AppsAPI() {
    m_stopping = true;
    writeApplications();
    settings.sync();
    dispatchToMainThread([this] {
        auto frameBuffer = EPFrameBuffer::framebuffer();
        O_DEBUG("Waiting for other painting to finish...");
        while (frameBuffer->paintingActive()) {
            EPFrameBuffer::waitForLastUpdate();
        }
        QPainter painter(frameBuffer);
        auto rect = frameBuffer->rect();
        auto fm = painter.fontMetrics();
        O_INFO("Clearing screen...");
        painter.setPen(Qt::white);
        painter.fillRect(rect, Qt::black);
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::FullUpdate, true);
        EPFrameBuffer::waitForLastUpdate();
        painter.end();
        O_DEBUG("Stopping applications...");
        for (auto app : applications) {
            if (app->stateNoSecurityCheck() != Application::Inactive) {
                auto text = "Stopping " + app->displayName() + "...";
                O_DEBUG(text.toStdString().c_str());
                notificationAPI->drawNotificationText(text, Qt::white, Qt::black);
                EPFrameBuffer::waitForLastUpdate();
            }
            app->stopNoSecurityCheck();
        }
        O_INFO("Ensuring all applications have stopped...");
        for (auto app : applications) {
            app->waitForFinished();
            app->deleteLater();
        }
        applications.clear();
        QPainter painter2(frameBuffer);
        O_INFO("Displaying final quit message...");
        painter2.fillRect(rect, Qt::black);
        painter2.setPen(Qt::white);
        if(systemAPI->landscape()){
            auto x = rect.width() / 2;
            auto y = rect.height() / 2;
            painter2.translate(x, y);
            painter2.rotate(90.0);
            painter2.translate(-x, -y);
        }
        painter2.drawText(rect, Qt::AlignCenter, "Goodbye!");
        EPFrameBuffer::waitForLastUpdate();
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::FullUpdate, true);
        painter2.end();
        EPFrameBuffer::waitForLastUpdate();
    });
}
#include "moc_appsapi.cpp"
