#include <liboxide.h>

#include <QCommandLineParser>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide::JSON;
using namespace Oxide::Applications;

#define LOG_VERBOSE(msg)                                                       \
  if (!parser.isSet(quietOption) && parser.isSet(verboseOption)) {             \
    qDebug() << msg;                                                           \
  }
#define LOG(msg)                                                               \
  if (!parser.isSet(quietOption)) {                                            \
    qDebug() << msg;                                                           \
  }

int
qExit(int ret) {
  QTimer::singleShot(0, [ret]() { qApp->exit(ret); });
  return qApp->exec();
}

QList<QString> configDirectoryPaths =
  {"/opt/etc/draft", "/etc/draft", "/home/root/.config/draft"};

int
main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  sentry_init("update-desktop-database", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("update-desktop-database");
  app.setApplicationVersion(APP_VERSION);
  QCommandLineParser parser;
  parser.setApplicationDescription(
    "Reload the application registration cache for Oxide"
  );
  parser.applicationDescription();
  parser.addHelpOption();
  parser.addPositionalArgument(
    "directory", "NOT IMPLEMENTED", "[DIRECTORY...]"
  );
  // TODO handle using $XDG_DATA_DIRS/applications if directory is not set
  QCommandLineOption versionOption("version", "Display the version and exit");
  parser.addOption(versionOption);
  QCommandLineOption quietOption(
    {"q", "quiet"},
    "Do not display any information about processing and updating progress."
  );
  parser.addOption(quietOption);
  QCommandLineOption verboseOption(
    {"v", "verbose"},
    "Display more information about processing and upating progress"
  );
  parser.addOption(verboseOption);
  parser.process(app);
  if (parser.isSet(versionOption)) {
    parser.showVersion();
  }
  auto bus = QDBusConnection::systemBus();
  General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  LOG_VERBOSE("Requesting apps API");
  QDBusObjectPath path = api.requestAPI("apps");
  if (path.path() == "/") {
    LOG("Unable to get apps API");
    return qExit(EXIT_FAILURE);
  }
  Apps apps(OXIDE_SERVICE, path.path(), bus);
  LOG("Loading applications from disk");
  QDir dir(OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY);
  dir.setNameFilters(QStringList() << "*.oxide");
  for (auto entry : dir.entryInfoList()) {
    auto path = entry.filePath();
    auto reg = getRegistration(path);
    auto name = entry.completeBaseName();
    auto errors = validateRegistration(name, reg);
    bool cache = true;
    for (auto error : errors) {
      if (
        error.level == ErrorLevel::Error || error.level == ErrorLevel::Critical
      ) {
        LOG_VERBOSE("  " << path.toStdString().c_str() << ": " << error);
        cache = false;
      }
    }
    if (cache && !addToTarnishCache(name, reg)) {
      LOG("  " << path << ": Failed to cache")
    }
  }
  LOG_VERBOSE("Finished reloading applications");
  LOG("Importing Draft Applications");
  for (auto configDirectoryPath : configDirectoryPaths) {
    QDir configDirectory(configDirectoryPath);
    configDirectory.setFilter(
      QDir::Files | QDir::NoSymLinks | QDir::NoDot | QDir::NoDotDot
    );
    auto images =
      configDirectory.entryInfoList(QDir::NoFilter, QDir::SortFlag::Name);
    for (QFileInfo fi : images) {
      if (fi.fileName() != "conf") {
        auto f = fi.absoluteFilePath();
        LOG_VERBOSE("parsing file " << f);
        QFile file(fi.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
          if (!parser.isSet(quietOption)) {
            qCritical() << "Couldn't find the file " << f;
          }
          continue;
        }
        QTextStream in(&file);
        QVariantMap properties;
        while (!in.atEnd()) {
          QString line = in.readLine();
          if (line.startsWith("#") || line.trimmed().isEmpty()) {
            continue;
          }
          QStringList parts = line.split("=");
          if (parts.length() != 2) {
            if (!parser.isSet(quietOption)) {
              O_WARNING("wrong format on " << line);
            }
            continue;
          }
          QString lhs = parts.at(0);
          QString rhs = parts.at(1);
          if (rhs != ":" && rhs != "") {
            if (lhs == "name") {
              properties.insert("name", rhs);
            } else if (lhs == "desc") {
              properties.insert("description", rhs);
            } else if (lhs == "imgFile") {
              auto icon = configDirectoryPath + "/icons/" + rhs + ".png";
              if (icon.startsWith("qrc:")) {
                icon = "";
              }
              properties.insert("icon", icon);
            } else if (lhs == "call") {
              properties.insert("bin", rhs);
            } else if (lhs == "term") {
              properties.insert("onStop", rhs.trimmed());
            }
          }
        }
        file.close();
        if (!properties.contains("name")) {
          LOG_VERBOSE("No name in properties");
          LOG("Failed to import");
          continue;
        }
        auto name = properties["name"].toString();
        path = apps.getApplicationPath(name);
        if (path.path() != "/") {
          LOG_VERBOSE("Already exists" << name);
          auto icon = properties["icon"].toString();
          if (icon.isEmpty()) {
            continue;
          }
          Application application(OXIDE_SERVICE, path.path(), bus);
          if (application.icon().isEmpty()) {
            application.setIcon(icon);
          }
          continue;
        }
        LOG_VERBOSE("Not found, creating...");
        if (Oxide::debugEnabled()) {
          LOG_VERBOSE(properties);
        }
        properties.insert("displayName", name);
        path = apps.registerApplication(properties);
        if (path.path() == "/") {
          LOG("Failed to import" << name);
        }
      }
    }
  }
  LOG_VERBOSE("Finished Importing Draft Applications");
  LOG("Importing AppLoad External Applications");
  QList<QString> apploadPaths = {"/home/root/xovi/exthome/appload"};
  for (auto apploadPath : apploadPaths) {
    QDir apploadDir(apploadPath);
    if (!apploadDir.exists()) {
      LOG_VERBOSE("AppLoad directory not found: " << apploadPath);
      continue;
    }
    for (auto appDirInfo : apploadDir.entryInfoList(
           QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name
         )) {
      auto appDir = appDirInfo.absoluteFilePath();
      auto manifestPath = appDir + "/external.manifest.json";
      if (!QFile::exists(manifestPath)) {
        if (QFile::exists(appDir + "/manifest.json")) {
          LOG_VERBOSE(
            "Skipping QML AppLoad app (needs AppLoad runtime): "
            << appDirInfo.fileName()
          );
        }
        continue;
      }
      LOG_VERBOSE("Processing " << manifestPath);
      QFile manifestFile(manifestPath);
      if (!manifestFile.open(QIODevice::ReadOnly)) {
        LOG("Failed to open manifest: " << manifestPath);
        continue;
      }
      auto variant = Oxide::JSON::fromJson(&manifestFile);
      manifestFile.close();
      auto manifest = variant.toMap();
      auto displayName = manifest["name"].toString();
      auto application = manifest["application"].toString();
      if (displayName.isEmpty() || application.isEmpty()) {
        LOG("Skipping " << manifestPath << ": missing name or application");
        continue;
      }
      QFileInfo appFileInfo(application);
      if (appFileInfo.isRelative()) {
        application = appDir + "/" + application;
      }
      QVariantMap properties;
      properties.insert("name", displayName);
      properties.insert("displayName", displayName);
      properties.insert("workingDirectory", appDir);
      properties.insert("flags", QStringList() << "nopreload");
      auto iconPath = appDir + "/icon.png";
      if (QFile::exists(iconPath)) {
        properties.insert("icon", iconPath);
      }
      QVariantMap env;
      auto args = manifest.value("args");
      properties.insert("bin", "/home/root/.vellum/share/oxide/libexec/runner");
      env.insert("EXECUTABLE", application);
      if (
        args.isValid() && args.canConvert<QVariantList>() &&
        !args.toList().isEmpty()
      ) {
        QStringList quotedArgs;
        for (auto arg : args.toList()) {
          auto a = arg.toString();
          a.replace("\\", "\\\\");
          a.replace("\"", "\\\"");
          a.replace("`", "\\`");
          a.replace("$", "\\$");
          quotedArgs << "\"" + a + "\"";
        }
        env.insert("ARGUMENTS", quotedArgs.join(" "));
      }
      auto environment = manifest.value("environment");
      if (environment.isValid() && environment.canConvert<QVariantMap>()) {
        auto environmentMap = environment.toMap();
        for (auto key : environmentMap.keys()) {
          auto value = environmentMap[key].toString();
          if (key == "LD_PRELOAD") {
            QStringList entries = value.split(":", Qt::SkipEmptyParts);
            QStringList kept;
            for (auto entry : entries) {
              auto filename = QFileInfo(entry).fileName();
              if (filename == "qtfb-shim-32bit.so") {
                env.insert("OXIDE_PRELOAD_FORCE_32BIT_FSCREENINFO", "1");
                continue;
              }
              if (filename != "qtfb-shim.so") {
                kept << entry;
              }
            }
            if (!kept.isEmpty()) {
              env.insert("LD_PRELOAD", kept.join(":"));
            }
            continue;
          }
          if (key == "QTFB_SHIM_MODEL") {
            if (value == "1" || value == "RM1") {
              env.insert("OXIDE_PRELOAD_FORCE_RM1", "1");
              continue;
            } else if (value == "RM2") {
              env.insert("OXIDE_PRELOAD_FORCE_RM2_NAME", "1");
              continue;
            }
          }
          if (key == "QTFB_SHIM_INPUT" && value == "0") {
            env.insert("OXIDE_PRELOAD_EXPOSE_INPUT", "1");
            continue;
          }
          if (key == "QTFB_SHIM_FB" && value == "0") {
            env.insert("OXIDE_PRELOAD_EXPOSE_FB", "1");
            continue;
          }
          if (key == "QTFB_SHIM_MODE" && value.toLower() == "rgb565") {
            env.insert("OXIDE_PRELOAD_FORCE_RGB16", "1");
            continue;
          }
          if (key == "QTFB_SHIM_INPUT_MODE" && value == "RM1") {
            env.insert("OXIDE_PRELOAD_FORCE_RM1_INPUT", "1");
            continue;
          }
          if (key == "QTFB_SHIM_INITIAL_DISPLAY_MODE") {
            env.insert("OXIDE_PRELOAD_FORCE_WAVEFORM", value);
            continue;
          }
          if (key.startsWith("QTFB_")) {
            LOG_VERBOSE(
              "Unhandled AppLoad environment variable: " << key << "=" << value
            );
            continue;
          }
          env.insert(key, value);
        }
      }
      if (!env.isEmpty()) {
        properties.insert("environment", env);
      }
      auto registrationPath =
        QString(OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY) + "/" + displayName +
        ".oxide";
      if (QFile::exists(registrationPath)) {
        LOG_VERBOSE("Already registered: " << displayName);
        continue;
      }
      LOG_VERBOSE("Not found, creating...");
      QFile registrationFile(registrationPath);
      if (!registrationFile.open(QIODevice::WriteOnly)) {
        LOG("Failed to create registration: " << registrationPath);
        continue;
      }
      registrationFile.write(
        QJsonDocument(QJsonObject::fromVariantMap(properties)).toJson()
      );
      LOG_VERBOSE("Imported AppLoad app: " << displayName);
    }
  }
  LOG_VERBOSE("Finished Importing AppLoad External Applications");
  apps.reload().waitForFinished();
  return qExit(EXIT_SUCCESS);
}
