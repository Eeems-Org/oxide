#include <liboxide.h>
#include <liboxide/applications.h>
#include <sys/stat.h>

#include <QCommandLineParser>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrlQuery>
#include <QUuid>
#include <string>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

int
launchAppByPath(
  const QDBusObjectPath& appPath,
  const QStringList& launchArgs = {}
) {
  auto bus = QDBusConnection::systemBus();
  General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  QDBusObjectPath path = api.requestAPI("apps");
  if (path.path() == "/") {
    qDebug() << "Unable to get apps API";
    return EXIT_FAILURE;
  }
  Apps apps(OXIDE_SERVICE, path.path(), bus);
  Application app(OXIDE_SERVICE, appPath.path(), bus);
  if (!app.forking()) {
    app.launchWithArgs(launchArgs).waitForFinished();
    return EXIT_SUCCESS;
  }
  // Register a transient background clone and launch it
  QVariantMap properties = {
    {"name",             QUuid::createUuid().toString(QUuid::Id128)},
    {"bin",              app.bin()                                 },
    {"displayName",      app.displayName()                         },
    {"description",      app.description()                         },
    {"type",             (int)ApplicationType::Background          },
    {"icon",             app.icon()                                },
    {"workingDirectory", app.workingDirectory()                    },
    {"user",             app.user()                                },
    {"group",            app.group()                               },
    {"environment",      app.environment()                         },
    {"permissions",      app.permissions()                         },
    {"directories",      app.directories()                         },
    {"onPause",          app.onPause()                             },
    {"onResume",         app.onResume()                            },
    {"onStop",           app.onStop()                              }
  };
  QStringList flags = app.flags();
  if (!flags.contains("transient")) {
    flags.prepend("transient");
  }
  flags.removeAll("forking");
  properties["flags"] = flags;
  QDBusObjectPath clonePath = apps.registerApplication(properties);
  if (clonePath.path() == "/") {
    qDebug() << "Failed to register transient instance of"
             << app.name().toStdString().c_str();
    return EXIT_FAILURE;
  }
  Application(OXIDE_SERVICE, clonePath.path(), bus)
    .launchWithArgs(launchArgs)
    .waitForFinished();
  return EXIT_SUCCESS;
}

int
launchOxideApp(const QString& name, const QStringList& launchArgs = {}) {
  auto bus = QDBusConnection::systemBus();
  General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  QDBusObjectPath path = api.requestAPI("apps");
  if (path.path() == "/") {
    qDebug() << "Unable to get apps API";
    return EXIT_FAILURE;
  }
  Apps apps(OXIDE_SERVICE, path.path(), bus);
  path = apps.getApplicationPath(name);
  if (path.path() == "/") {
    qDebug() << "Application does not exist:" << name.toStdString().c_str();
    return EXIT_FAILURE;
  }
  return launchAppByPath(path, launchArgs);
}

int
openFileWithMime(const QString& filePath) {
  auto bus = QDBusConnection::systemBus();
  General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  QDBusObjectPath path = api.requestAPI("apps");
  if (path.path() == "/") {
    qDebug() << "Unable to get apps API";
    return EXIT_FAILURE;
  }
  Apps apps(OXIDE_SERVICE, path.path(), bus);
  QString mimeType =
    QMimeDatabase().mimeTypeForFile(QFileInfo(filePath)).name();
  QDBusObjectPath defaultApp = apps.defaultApplication(mimeType);
  if (defaultApp.path() != "/") {
    return launchAppByPath(defaultApp, {filePath});
  }
  QList<QDBusObjectPath> candidates = apps.applicationsForMimeType(mimeType);
  if (candidates.size() == 1) {
    return launchAppByPath(candidates.first(), {filePath});
  }
  if (candidates.size() == 0) {
    qDebug() << "No application found for MIME type:" << mimeType;
    return EXIT_FAILURE;
  }
  return launchOxideApp("codes.eeems.openwith", {mimeType, filePath});
}

int
main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  sentry_init("xdg-open", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("xdg-open");
  app.setApplicationVersion(APP_VERSION);
  QCommandLineParser parser;
  parser.setApplicationDescription("Open a file or URL.");
  parser.applicationDescription();
  parser.addHelpOption();
  parser.addVersionOption();
  parser.process(app);
  QStringList args = parser.positionalArguments();
  if (args.isEmpty() || args.length() > 1) {
    parser.showHelp(EXIT_FAILURE);
  }
  auto path = args.first();
  auto url =
    QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
  if (url.scheme().isEmpty()) {
    url.setScheme("file");
  }
  if (url.isLocalFile()) {
    QFileInfo info(url.path());
    if (info.suffix() != "oxide") {
      return openFileWithMime(url.path());
    }
    if (
      info.absoluteDir().path() != OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY
    ) {
      qDebug() << "The registration file must be "
                  "in " OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY ":"
               << path.toStdString().c_str();
      return EXIT_FAILURE;
    }
    // Workaround because basename will sometimes return the suffix instead
    auto name = info.fileName().left(info.fileName().length() - 6);
    return launchOxideApp(name);
  } else if (url.scheme() == "oxide") {
    if (
      url.hasFragment() || !url.userInfo().isEmpty() ||
      !url.authority().isEmpty()
    ) {
      qDebug() << "Url must be in the format oxide:{appname}[?arg=...] :"
               << path.toStdString().c_str();
      return EXIT_FAILURE;
    }
    auto name = url.path();
    QStringList launchArgs;
    if (url.hasQuery()) {
      QUrlQuery query(url);
      launchArgs = query.allQueryItemValues("arg");
    }
    return launchOxideApp(name, launchArgs);
  }
  qDebug() << "Operation not supported:" << path.toStdString().c_str();
  return EXIT_FAILURE;
}
