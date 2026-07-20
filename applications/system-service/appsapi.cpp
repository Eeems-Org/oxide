#include "appsapi.h"

#include <libblight.h>
#include <liboxide.h>
#include <liboxide/oxideqml.h>

#include "application.h"
#include "notificationapi.h"
#include "systemapi.h"

using namespace Oxide;

AppsAPI*
AppsAPI::singleton(AppsAPI* self) {
  static AppsAPI* instance;
  if (self != nullptr) {
    instance = self;
  }
  return instance;
}

AppsAPI::AppsAPI(QObject* parent)
  : APIBase(parent)
  , m_stopping(false)
  , m_starting(true)
  , m_enabled(false)
  , applications()
  , previousApplications()
  , settings(this)
  , m_sleeping(false) {
  Oxide::Sentry::sentry_transaction(
    "Init Apps API", "init", [this](Oxide::Sentry::Transaction* t) {
      Oxide::Sentry::sentry_span(
        t,
        "start",
        "Launching initial application",
        [this](Oxide::Sentry::Span* s) {
          Oxide::Sentry::sentry_span(s, "singleton", "Setup singleton", [this] {
            singleton(this);
          });
          Oxide::Sentry::sentry_span(
            s, "signals", "Setup unix signal handlers", [] {
              SignalHandler::setup_unix_signal_handlers();
            }
          );
          qDBusRegisterMetaType<QMap<QString, QDBusObjectPath>>();
          qDBusRegisterMetaType<QDBusObjectPath>();
          Oxide::Sentry::sentry_span(s, "sync", "Sync settings", [this] {
            settings.sync();
          });
          Oxide::Sentry::sentry_span(
            s, "migrate", "Migrate to latest version", [this] {
              migrate(&settings, settings.value("version", 0).toInt());
            }
          );
        }
      );
      Oxide::Sentry::sentry_span(
        t, "application", "Read applications from disk", [this] {
          readApplications();
        }
      );
    }
  );
}

void
AppsAPI::startup() {
  Oxide::Sentry::sentry_transaction(
    "Apps API Startup", "startup", [this](Oxide::Sentry::Transaction* t) {
      if (applications.isEmpty()) {
        O_INFO("No applications found");
        notificationAPI->errorNotification(_noApplicationsMessage);
        return;
      }
      Oxide::Sentry::sentry_span(
        t,
        "autoStart",
        "Launching auto start applications",
        [this](Oxide::Sentry::Span* s) {
          for (auto app : applications) {
            if (!app->autoStart()) {
              continue;
            }
            O_INFO("Auto starting" << app->name());
            Oxide::Sentry::sentry_span(
              s, app->name().toStdString(), "Launching application", [app] {
                app->launchNoSecurityCheck();
                if (app->type() == Backgroundable) {
                  O_INFO("  Pausing auto started app" << app->name());
                  app->pauseNoSecurityCheck();
                }
              }
            );
          }
        }
      );
      Oxide::Sentry::sentry_span(
        t, "start", "Launching initial application", [this] {
          Application* app = getDefaultApplication("lockscreen");
          if (app == nullptr) {
            O_WARNING("Could not find lockscreen application");
            app = getDefaultApplication("launcher");
          }
          if (app == nullptr) {
            O_WARNING("Could not find launcher application");
            app = getApplication("codes.eeems.decay");
          }
          if (app == nullptr) {
            O_WARNING("Could not find backup lockscreen application");
            app = getApplication("codes.eeems.oxide");
          }
          if (app == nullptr) {
            O_WARNING("Could not find backup launcher application");
            O_WARNING("Using xochitl due to invalid configuration");
            app = getApplication("xochitl");
          }
          if (app == nullptr) {
            O_WARNING("Could not find xochitl");
            O_WARNING(
              "Using the first application in the list due to "
              "invalid configuration"
            );
            app = applications.first();
          }
          O_INFO("Starting initial application" << app->name());
          app->launchNoSecurityCheck();
          ensureForegroundApp();
        }
      );
    }
  );
}

void
AppsAPI::setEnabled(bool enabled) {
  O_INFO("Apps API" << enabled);
  for (auto app : applications) {
    if (enabled) {
      app->registerPath();
    } else {
      app->unregisterPath();
    }
  }
  m_enabled = enabled;
}

QDBusObjectPath
AppsAPI::registerApplication(QVariantMap properties) {
  if (!hasPermission("apps")) {
    return QDBusObjectPath("/");
  }
  if (properties.contains("environment")) {
    properties["environment"] =
      QVariant(qdbus_cast<QVariantMap>(properties.value("environment")));
  }
  return registerApplicationNoSecurityCheck(properties);
}

QDBusObjectPath
AppsAPI::registerApplicationNoSecurityCheck(QVariantMap properties) {
  QString name = properties.value("name", "").toString();
  QString bin = properties.value("bin", "").toString();
  int type = properties.value("type", ApplicationType::Foreground).toInt();
  if (
    type < ApplicationType::Foreground || type > ApplicationType::Backgroundable
  ) {
    O_WARNING("Invalid configuration: Invalid type" << type);
    return QDBusObjectPath("/");
  }
  if (name.isEmpty()) {
    O_WARNING("Invalid configuration: Name is empty");
    return QDBusObjectPath("/");
  }
  if (bin.isEmpty() || !QFile::exists(bin)) {
    O_WARNING("Invalid configuration: " << name << " has invalid bin" << bin);
    return QDBusObjectPath("/");
  }
  if (!QFileInfo(bin).isExecutable()) {
    O_WARNING(
      "Invalid configuration: " << name << " has bin that is not executable"
                                << bin
    );
    return QDBusObjectPath("/");
  }
  if (applications.contains(name)) {
    return applications[name]->qPath();
  }
  QDBusObjectPath path;
  Oxide::Sentry::sentry_transaction(
    "Register Application",
    "registerApplication",
    [this, &path, name, properties](Oxide::Sentry::Transaction* t) {
      Q_UNUSED(t);
      path = QDBusObjectPath(getPath(name));
      auto app = new Application(path, reinterpret_cast<QObject*>(this));
      auto displayName = properties.value("displayName", name).toString();
      app->setConfig(properties);
      applications.insert(name, app);
      app->registerPath();
      emit applicationRegistered(path);
    }
  );
  return path;
}

bool
AppsAPI::unregisterApplication(QDBusObjectPath path) {
  if (!hasPermission("apps")) {
    return false;
  }
  auto app = getApplication(path);
  if (app == nullptr) {
    return true;
  }
  if (app->systemApp()) {
    return false;
  }
  unregisterApplication(app);
  return true;
}

void
AppsAPI::reload() {
  if (!hasPermission("apps")) {
    return;
  }
  Oxide::Sentry::sentry_transaction(
    "Reload Apps", "reload", [this](Oxide::Sentry::Transaction* t) {
      Q_UNUSED(t);
      writeApplications();
      readApplications();
    }
  );
}

Application*
AppsAPI::getDefaultApplication() {
  Application* app = getDefaultApplication("launcher");
  if (app != nullptr) {
    return app;
  }
  app = getApplication("codes.eeems.oxide");
  if (app != nullptr) {
    return app;
  }
  return getApplication("xochitl");
}

QDBusObjectPath
AppsAPI::startupApplication() {
  return defaultApplication("launcher");
}

void
AppsAPI::setStartupApplication(QDBusObjectPath path) {
  setDefaultApplication("launcher", path);
}

QDBusObjectPath
AppsAPI::lockscreenApplication() {
  return defaultApplication("lockscreen");
}

void
AppsAPI::setLockscreenApplication(QDBusObjectPath path) {
  if (!hasPermission("apps")) {
    return;
  }
  setDefaultApplication("lockscreen", path);
}

QDBusObjectPath
AppsAPI::processManagerApplication() {
  return defaultApplication("process-manager");
}

void
AppsAPI::setProcessManagerApplication(QDBusObjectPath path) {
  setDefaultApplication("process-manager", path);
}

QDBusObjectPath
AppsAPI::taskSwitcherApplication() {
  return defaultApplication("task-switcher");
}

void
AppsAPI::setTaskSwitcherApplication(QDBusObjectPath path) {
  setDefaultApplication("task-switcher", path);
}

QVariantMap
AppsAPI::getApplications() {
  QVariantMap result;
  if (!hasPermission("apps")) {
    return result;
  }
  for (auto app : applications) {
    result.insert(app->name(), QVariant::fromValue(app->qPath()));
  }
  return result;
}

QDBusObjectPath
AppsAPI::currentApplication() {
  if (!hasPermission("apps")) {
    return QDBusObjectPath("/");
  }
  return currentApplicationNoSecurityCheck();
}

QDBusObjectPath
AppsAPI::currentApplicationNoSecurityCheck() {
  for (auto app : applications) {
    if (app->stateNoSecurityCheck() == Application::InForeground) {
      return app->qPath();
    }
  }
  return QDBusObjectPath("/");
}

QVariantMap
AppsAPI::runningApplications() {
  if (!hasPermission("apps")) {
    return QVariantMap();
  }
  return runningApplicationsNoSecurityCheck();
}

QVariantMap
AppsAPI::runningApplicationsNoSecurityCheck() {
  QVariantMap result;
  for (auto app : applications) {
    auto state = app->stateNoSecurityCheck();
    if (
      state == Application::InForeground || state == Application::InBackground
    ) {
      result.insert(app->name(), QVariant::fromValue(app->qPath()));
    }
  }
  return result;
}

QVariantMap
AppsAPI::pausedApplications() {
  QVariantMap result;
  if (!hasPermission("apps")) {
    return result;
  }
  for (auto app : applications) {
    auto state = app->stateNoSecurityCheck();
    if (state == Application::Paused) {
      result.insert(app->name(), QVariant::fromValue(app->qPath()));
    }
  }
  return result;
}

void
AppsAPI::unregisterApplication(Application* app) {
  Oxide::Sentry::sentry_transaction(
    "Unregister Application",
    "unregisterApplication",
    [this, app](Oxide::Sentry::Transaction* t) {
      Q_UNUSED(t);
      auto name = app->name();
      if (applications.contains(name)) {
        applications.remove(name);
        emit applicationUnregistered(app->qPath());
        app->deleteLater();
      }
    }
  );
}

void
AppsAPI::pauseAll() {
  for (auto app : applications) {
    app->pauseNoSecurityCheck(false);
  }
}

void
AppsAPI::resumeIfNone() {
  if (m_stopping || m_starting) {
    return;
  }
  for (auto app : applications) {
    if (app->stateNoSecurityCheck() == Application::InForeground) {
      return;
    }
  }
  if (previousApplicationNoSecurityCheck()) {
    return;
  }
  auto app = getDefaultApplication();
  if (app == nullptr) {
    if (applications.isEmpty()) {
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

Application*
AppsAPI::getApplication(QDBusObjectPath path) {
  if (path.path() == "/") {
    return nullptr;
  }
  for (auto app : applications) {
    if (app->path() == path.path()) {
      return app;
    }
  }
  return nullptr;
}

QStringList
AppsAPI::getPreviousApplications() {
  return previousApplications;
}

QDBusObjectPath
AppsAPI::getApplicationPath(const QString& name) {
  if (!hasPermission("apps")) {
    return QDBusObjectPath("/");
  }
  auto app = getApplication(name);
  if (app == nullptr) {
    return QDBusObjectPath("/");
  }
  return app->qPath();
}

Application*
AppsAPI::getApplication(const QString& name) {
  if (applications.contains(name)) {
    return applications[name];
  }
  return nullptr;
}

void
AppsAPI::connectSignals(Application* app, int signal) {
  switch (signal) {
    case 1:
      connect(
        signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1
      );
      break;
    case 2:
      connect(
        signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2
      );
      break;
  }
}

void
AppsAPI::disconnectSignals(Application* app, int signal) {
  switch (signal) {
    case 1:
      disconnect(
        signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1
      );
      break;
    case 2:
      disconnect(
        signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2
      );
      break;
  }
}

bool
AppsAPI::previousApplication() {
  if (!hasPermission("apps")) {
    return false;
  }
  return previousApplicationNoSecurityCheck();
}

bool
AppsAPI::previousApplicationNoSecurityCheck() {
  if (locked()) {
    return false;
  }
  if (previousApplications.isEmpty()) {
    O_DEBUG("No previous applications");
    return false;
  }
  bool found = false;
  while (!previousApplications.isEmpty()) {
    auto name = previousApplications.takeLast();
    auto application = getApplication(name);
    if (application == nullptr) {
      continue;
    }
    auto currentApplication =
      getApplication(this->currentApplicationNoSecurityCheck());
    if (currentApplication != nullptr) {
      if (currentApplication == application) {
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

void
AppsAPI::forceRecordPreviousApplication() {
  auto currentApplication =
    getApplication(this->currentApplicationNoSecurityCheck());
  if (currentApplication == nullptr) {
    O_WARNING("Unable to find current application");
    return;
  }
  auto name = currentApplication->name();
  previousApplications.removeAll(name);
  previousApplications.append(name);
  O_DEBUG("Previous Applications" << previousApplications);
}

void
AppsAPI::recordPreviousApplication() {
  auto currentApplication =
    getApplication(this->currentApplicationNoSecurityCheck());
  if (currentApplication == nullptr) {
    O_WARNING("Unable to find current application");
    return;
  }
  if (currentApplication->qPath() == lockscreenApplication()) {
    return;
  }
  if (currentApplication->qPath() == taskSwitcherApplication()) {
    return;
  }
  auto name = currentApplication->name();
  removeFromPreviousApplications(name);
  previousApplications.append(name);
  O_DEBUG("Previous Applications" << previousApplications);
}

void
AppsAPI::removeFromPreviousApplications(QString name) {
  previousApplications.removeAll(name);
}

void
AppsAPI::leftHeld() {
  openDefaultApplication();
}

void
AppsAPI::openDefaultApplication() {
  if (locked() || !hasPermission("apps")) {
    return;
  }
  if (onDefault() || onLockscreen()) {
    O_WARNING("Already in the default application");
    return;
  }
  Application* app = getDefaultApplication();
  if (app != nullptr) {
    O_INFO("Opening default application");
    app->launchNoSecurityCheck();
    return;
  }
  O_WARNING("Unable to find default application");
}

void
AppsAPI::homeHeld() {
  openTaskManager();
}

bool
AppsAPI::onLockscreen() {
  auto path = this->currentApplicationNoSecurityCheck();
  if (path.path() == "/") {
    return false;
  }
  Application* app = getDefaultApplication("lockscreen");
  if (app == nullptr) {
    app = getApplication("codes.eeems.decay");
  }
  if (app == nullptr) {
    return false;
  }
  auto currentApplication = getApplication(path);
  return currentApplication != nullptr &&
         currentApplication->stateNoSecurityCheck() != Application::Inactive &&
         path == app->qPath();
}

bool
AppsAPI::onTaskSwitcher() {
  auto path = this->currentApplicationNoSecurityCheck();
  if (path.path() == "/") {
    return false;
  }
  Application* app = getDefaultApplication("task-switcher");
  if (app == nullptr) {
    app = getApplication("codes.eeems.corrupt");
  }
  if (app == nullptr) {
    return false;
  }
  auto currentApplication = getApplication(path);
  return currentApplication != nullptr &&
         currentApplication->stateNoSecurityCheck() != Application::Inactive &&
         path == app->qPath();
}

bool
AppsAPI::onDefault() {
  auto path = this->currentApplicationNoSecurityCheck();
  if (path.path() == "/") {
    return false;
  }
  auto app = getDefaultApplication("launcher");
  if (app == nullptr) {
    app = getApplication("codes.eeems.oxide");
  }
  if (app == nullptr) {
    app = getApplication("xochitl");
  }
  if (app == nullptr) {
    return false;
  }
  auto currentApplication = getApplication(path);
  return currentApplication != nullptr &&
         currentApplication->stateNoSecurityCheck() != Application::Inactive &&
         path == app->qPath();
}

void
AppsAPI::openTaskManager() {
  if (locked() || !hasPermission("apps")) {
    return;
  }
  if (onLockscreen()) {
    O_WARNING("Can't open task manager, on the lockscreen");
    return;
  }
  Application* app = getDefaultApplication("process-manager");
  if (app != nullptr) {
    O_INFO("Opening task manager");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("codes.eeems.erode");
  if (app != nullptr) {
    O_INFO("Opening task manager");
    app->launchNoSecurityCheck();
    return;
  }
  O_WARNING("Unable to find task manager");
}

void
AppsAPI::openLockScreen() {
  if (locked() || !hasPermission("apps")) {
    return;
  }
  if (onLockscreen()) {
    O_WARNING("Already on the lockscreen");
    return;
  }
  Application* app = getDefaultApplication("lockscreen");
  if (app != nullptr) {
    O_INFO("Opening lock screen");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("codes.eeems.decay");
  if (app != nullptr) {
    O_INFO("Opening lock screen");
    app->launchNoSecurityCheck();
    return;
  }
  O_WARNING("Unable to find lockscreen");
}

void
AppsAPI::openTaskSwitcher() {
  if (locked() || !hasPermission("apps")) {
    return;
  }
  if (onLockscreen()) {
    O_WARNING("Can't open task switcher, on the lockscreen");
    return;
  }
  if (onTaskSwitcher()) {
    O_WARNING("Already on the task switcher");
    return;
  }
  Application* app = getDefaultApplication("task-switcher");
  if (app != nullptr) {
    O_INFO("Opening task switcher");
    app->launchNoSecurityCheck();
    return;
  }
  app = getDefaultApplication("launcher");
  if (app != nullptr) {
    O_INFO("Opening task switcher");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("codes.eeems.corrupt");
  if (app != nullptr) {
    O_INFO("Opening task switcher");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("codes.eeems.oxide");
  if (app != nullptr) {
    O_INFO("Opening task switcher");
    app->launchNoSecurityCheck();
    return;
  }
  O_WARNING("Unable to find task switcher");
  return;
}

void
AppsAPI::openTerminal() {
  if (locked() || !hasPermission("apps")) {
    return;
  }
  if (onLockscreen()) {
    O_WARNING("Can't open terminal, on the lockscreen");
    return;
  }
  Application* app = getDefaultApplication("terminal");
  if (app != nullptr) {
    O_INFO("Opening terminal");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("yaft");
  if (app != nullptr) {
    O_INFO("Opening terminal");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("YAFT");
  if (app != nullptr) {
    O_INFO("Opening terminal");
    app->launchNoSecurityCheck();
    return;
  }
  app = getApplication("fingerterm");
  if (app != nullptr) {
    O_INFO("Opening terminal");
    app->launchNoSecurityCheck();
    return;
  }
  O_WARNING("Unable to find terminal application");
}

void
AppsAPI::openSettings(const QString& category) {
  if (locked() || !hasPermission("apps")) {
    return;
  }
  Application* app = getDefaultApplication("settings");
  if (app == nullptr) {
    app = getApplication("codes.eeems.settings");
  }
  if (app == nullptr) {
    O_WARNING("Unable to find settings application");
    return;
  }
  QStringList args = category.isEmpty() ? QStringList{} : QStringList{category};
  if (!app->forking()) {
    app->launchNoSecurityCheck(args);
    return;
  }
  QVariantMap properties;
  properties["name"] = QUuid::createUuid().toString(QUuid::Id128);
  properties["bin"] = app->bin();
  properties["displayName"] = app->displayName();
  properties["description"] = app->description();
  properties["type"] = (int)ApplicationType::Background;
  properties["icon"] = app->icon();
  properties["workingDirectory"] = app->workingDirectory();
  properties["user"] = app->user();
  properties["group"] = app->group();
  properties["environment"] = app->environment();
  properties["permissions"] = app->permissions();
  properties["directories"] = app->directories();
  properties["onPause"] = app->onPause();
  properties["onResume"] = app->onResume();
  properties["onStop"] = app->onStop();
  QStringList cloneFlags = app->flags();
  if (!cloneFlags.contains("transient")) {
    cloneFlags.prepend("transient");
  }
  cloneFlags.removeAll("forking");
  properties["flags"] = cloneFlags;
  QDBusObjectPath clonePath = registerApplicationNoSecurityCheck(properties);
  if (clonePath.path() == "/") {
    O_WARNING("Failed to register transient settings instance");
    return;
  }
  auto clone = getApplication(clonePath);
  if (clone == nullptr) {
    return;
  }
  clone->launchNoSecurityCheck(args);
}

QDBusObjectPath
AppsAPI::application() {
  if (!hasPermission("apps")) {
    return QDBusObjectPath("/");
  }
  return APIBase::application();
}

QDBusObjectPath
AppsAPI::defaultApplication(const QString& type) {
  if (!hasPermission("apps")) {
    return QDBusObjectPath("/");
  }
  QString path = settings.value(QString("defaults/%1").arg(type)).toString();
  return QDBusObjectPath(path.isEmpty() ? "/" : path);
}

Application*
AppsAPI::getDefaultApplication(const QString& type) {
  QDBusObjectPath path = defaultApplication(type);
  return getApplication(path);
}

void
AppsAPI::setDefaultApplication(
  const QString& type,
  const QDBusObjectPath& path
) {
  if (!hasPermission("apps")) {
    return;
  }
  settings.setValue(QString("defaults/%1").arg(type), path.path());
}

QString
AppsAPI::getPath(QString name) {
  static const QUuid NS =
    QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));
  return QString(OXIDE_SERVICE_PATH) + "/apps/" +
         QUuid::createUuidV5(NS, name).toString(QUuid::Id128);
}

void
AppsAPI::writeApplications() {
  auto apps = applications.values();
  int size = apps.size();
  settings.beginWriteArray("applications", size);
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    auto app = apps[i];
    auto config = app->getConfig();
    // Add/Update config items
    for (auto key : config.keys()) {
      settings.setValue(key, config[key]);
    }
    // Remove old config
    for (auto key : settings.childKeys()) {
      if (!config.contains(key)) {
        settings.remove(key);
      }
    }
  }
  settings.endArray();
}

void
AppsAPI::readApplications() {
  settings.sync();
  if (!applications.empty()) {
    // Unregister any applications that have been removed from the settings
    // file
    int size = settings.beginReadArray("applications");
    QStringList names;
    for (int i = 0; i < size; ++i) {
      settings.setArrayIndex(i);
      names << settings.value("name").toString();
    }
    settings.endArray();
    for (auto name : applications.keys()) {
      auto app = applications[name];
      if (!names.contains(name) && !app->systemApp() && !app->transient()) {
        app->unregisterNoSecurityCheck();
      }
    }
  }
  // Register/Update applications from the settings file
  int size = settings.beginReadArray("applications");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    auto name = settings.value("name").toString();
    auto displayName = settings.value("displayName", name).toString();
    auto type = settings.value("type", Foreground).toInt();
    auto bin = settings.value("bin").toString();
    if (
      type < Foreground || type > Backgroundable || name.isEmpty() ||
      bin.isEmpty()
    ) {
      O_DEBUG("Invalid configuration " << name);
      continue;
    }
    QVariantMap properties{
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
      {"directories",
       settings.value("directories", QStringList()).toStringList()},
      {"permissions",
       settings.value("permissions", QStringList()).toStringList()},
    };
    if (settings.contains("user")) {
      properties.insert("user", settings.value("user", "").toString());
    }
    if (settings.contains("group")) {
      properties.insert("group", settings.value("group", "").toString());
    }
    if (applications.contains(name)) {
      O_DEBUG("Updating " << name);
      O_DEBUG(properties);
      applications[name]->setConfig(properties);
    } else {
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
  for (auto entry : dir.entryInfoList()) {
    auto app = getRegistration(entry.filePath());
    if (app.isEmpty()) {
      O_WARNING("Invalid file " << entry.filePath());
      continue;
    }
    auto name = entry.completeBaseName();
    app["name"] = name;
    apps.insert(name, app);
  }
  // Unregister any system applications that no longer exist on disk.
  for (auto application : applications.values()) {
    auto name = application->name();
    if (!apps.contains(name) && application->systemApp()) {
      O_WARNING(name << "Is no longer found on disk");
      application->unregisterNoSecurityCheck();
    }
  }
  // Register/Update any system application.
  for (auto app : apps) {
    auto name = app["name"].toString();
    auto bin = app["bin"].toString();
    if (bin.isEmpty() || !QFile::exists(bin)) {
      O_WARNING(name << "Can't find application binary:" << bin);
      O_DEBUG(app);
      continue;
    }
    if (!app.contains("flags") || !app["flags"].isArray()) {
      app["flags"] = QJsonArray();
    }
    auto flags = app["flags"].toArray();
    flags.prepend("system");
    app["flags"] = flags;
    auto properties = registrationToMap(app);
    if (applications.contains(name)) {
      O_DEBUG("Updating " << name);
      O_DEBUG(properties);
      applications[name]->setConfig(properties);
    } else {
      O_INFO("New system app" << name);
      O_DEBUG(properties);
      registerApplicationNoSecurityCheck(properties);
    }
  }
}

void
AppsAPI::migrate(QSettings* settings, int fromVersion) {
  if (fromVersion >= OXIDE_SETTINGS_VERSION) {
    return;
  }
  if (settings->contains("lockscreenApplication")) {
    settings->setValue(
      "defaults/lockscreen", settings->value("lockscreenApplication").toString()
    );
    settings->remove("lockscreenApplication");
  }
  if (settings->contains("startupApplication")) {
    settings->setValue(
      "defaults/launcher", settings->value("startupApplication").toString()
    );
    settings->remove("startupApplication");
  }
  if (settings->contains("processManagerApplication")) {
    settings->setValue(
      "defaults/process-manager",
      settings->value("processManagerApplication").toString()
    );
    settings->remove("processManagerApplication");
  }
  if (settings->contains("taskSwitcherApplication")) {
    settings->setValue(
      "defaults/task-switcher",
      settings->value("taskSwitcherApplication").toString()
    );
    settings->remove("taskSwitcherApplication");
  }
  settings->setValue("version", OXIDE_SETTINGS_VERSION);
  settings->sync();
}

bool
AppsAPI::locked() {
  return notificationAPI->locked();
}

void
AppsAPI::ensureForegroundApp() {
  QTimer::singleShot(300, [this] {
    m_starting = false;
    auto path = appsAPI->currentApplicationNoSecurityCheck();
    if (path.path() == "/") {
      notificationAPI->errorNotification(_noForegroundAppMessage);
      return;
    }
    auto app = appsAPI->getApplication(path);
    if (app == nullptr || app->state() == Application::Inactive) {
      notificationAPI->errorNotification(_noForegroundAppMessage);
      return;
    }
  });
}

void
AppsAPI::shutdown() {
  m_stopping = true;
  writeApplications();
  settings.sync();
  dispatchToMainThread([this] {
    Blight::shared_buf_t buffer = createBuffer();
    if (buffer != nullptr) {
      auto image = Oxide::QML::getImageForSurface(buffer);
      QPainter painter(&image);
      qDebug() << "Clearing screen...";
      painter.setPen(Qt::white);
      painter.fillRect(image.rect(), Qt::white);
      painter.end();
      addSystemBuffer(buffer);
    }
    O_DEBUG("Stopping applications...");
    auto notification = notificationAPI->paintNotification("", "");
    for (auto app : applications) {
      if (app->stateNoSecurityCheck() != Application::Inactive) {
        auto text = "Stopping " + app->displayName() + "...";
        O_DEBUG(text.toStdString().c_str());
        if (notification != nullptr) {
          notification->setProperty("text", text);
        }
      }
      app->stopNoSecurityCheck();
    }
    O_INFO("Ensuring all applications have stopped...");
    for (auto app : applications) {
      app->waitForFinished();
      app->deleteLater();
    }
    applications.clear();
    if (notification != nullptr) {
      notification->setProperty("notificationVisible", false);
    }
    if (buffer != nullptr) {
      auto image = Oxide::QML::getImageForSurface(buffer);
      QPainter painter(&image);
      O_INFO("Displaying final quit message...");
      auto rect = image.rect();
      painter.fillRect(rect, Qt::white);
      painter.setPen(Qt::black);
      if (systemAPI->landscape()) {
        auto x = rect.width() / 2;
        auto y = rect.height() / 2;
        painter.translate(x, y);
        painter.rotate(90.0);
        painter.translate(-x, -y);
      }
      painter.drawText(rect, Qt::AlignCenter, "Goodbye!");
      painter.end();
      auto maybe = Blight::connection()->repaint(buffer);
      if (maybe.has_value()) {
        maybe.value()->wait();
      }
    }
  });
}
#include "moc_appsapi.cpp"
