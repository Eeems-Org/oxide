#include <liboxide.h>
#include <liboxide/applications.h>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDebug>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUuid>
#include <string>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

int
main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  sentry_init("openwith", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("openwith");
  app.setApplicationVersion(APP_VERSION);

  QCommandLineParser parser;
  parser.setApplicationDescription("Choose an application to open a file");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("mimetype", "MIME type of the file");
  parser.addPositionalArgument("filepath", "Path to the file to open");
  parser.process(app);

  QStringList args = parser.positionalArguments();
  if (args.size() != 2) {
    qDebug() << "Usage: chooser <mimetype> <filepath>";
    return EXIT_FAILURE;
  }

  QString mimeType = args[0];
  QString filePath = args[1];

  auto bus = QDBusConnection::systemBus();
  General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  QDBusObjectPath path = api.requestAPI("apps");
  if (path.path() == "/") {
    qDebug() << "Unable to get apps API";
    return EXIT_FAILURE;
  }
  Apps apps(OXIDE_SERVICE, path.path(), bus);

  // Find applications that handle this MIME type
  QList<QDBusObjectPath> candidates = apps.applicationsForMimeType(mimeType);
  if (candidates.isEmpty()) {
    qDebug() << "No applications found for MIME type:" << mimeType;
    return EXIT_FAILURE;
  }

  if (candidates.size() == 1) {
    // Only one candidate, launch it directly
    Application chosen(OXIDE_SERVICE, candidates.first().path(), bus);
    QStringList launchArgs = {filePath};
    if (!chosen.forking()) {
      chosen.launchWithArgs(launchArgs).waitForFinished();
      return EXIT_SUCCESS;
    }
    // Register transient clone
    QVariantMap properties;
    properties["name"] = QUuid::createUuid().toString(QUuid::Id128);
    properties["bin"] = chosen.bin();
    properties["displayName"] = chosen.displayName();
    properties["description"] = chosen.description();
    properties["type"] = (int)ApplicationType::Background;
    properties["icon"] = chosen.icon();
    properties["workingDirectory"] = chosen.workingDirectory();
    properties["user"] = chosen.user();
    properties["group"] = chosen.group();
    properties["environment"] = chosen.environment();
    properties["permissions"] = chosen.permissions();
    properties["directories"] = chosen.directories();
    properties["onPause"] = chosen.onPause();
    properties["onResume"] = chosen.onResume();
    properties["onStop"] = chosen.onStop();
    QStringList cloneFlags = chosen.flags();
    if (!cloneFlags.contains("transient")) {
      cloneFlags.prepend("transient");
    }
    cloneFlags.removeAll("forking");
    properties["flags"] = cloneFlags;
    QDBusObjectPath clonePath = apps.registerApplication(properties);
    if (clonePath.path() == "/") {
      qDebug() << "Failed to register transient instance";
      return EXIT_FAILURE;
    }
    Application clone(OXIDE_SERVICE, clonePath.path(), bus);
    clone.launchWithArgs(launchArgs).waitForFinished();
    return EXIT_SUCCESS;
  }

  // Multiple candidates - show chooser dialog
  // For now, just pick the first one and log a message
  // TODO: Implement actual UI dialog
  qDebug() << "Multiple applications found for" << mimeType
           << "- launching first one";
  qDebug() << "Candidates:";
  for (auto& c : candidates) {
    Application a(OXIDE_SERVICE, c.path(), bus);
    qDebug() << "  " << a.displayName() << "(" << c.path() << ")";
  }

  Application chosen(OXIDE_SERVICE, candidates.first().path(), bus);
  QStringList launchArgs = {filePath};
  if (!chosen.forking()) {
    chosen.launchWithArgs(launchArgs).waitForFinished();
    return EXIT_SUCCESS;
  }
  // Register transient clone
  QVariantMap properties;
  properties["name"] = QUuid::createUuid().toString(QUuid::Id128);
  properties["bin"] = chosen.bin();
  properties["displayName"] = chosen.displayName();
  properties["description"] = chosen.description();
  properties["type"] = (int)ApplicationType::Background;
  properties["icon"] = chosen.icon();
  properties["workingDirectory"] = chosen.workingDirectory();
  properties["user"] = chosen.user();
  properties["group"] = chosen.group();
  properties["environment"] = chosen.environment();
  properties["permissions"] = chosen.permissions();
  properties["directories"] = chosen.directories();
  properties["onPause"] = chosen.onPause();
  properties["onResume"] = chosen.onResume();
  properties["onStop"] = chosen.onStop();
  QStringList cloneFlags = chosen.flags();
  if (!cloneFlags.contains("transient")) {
    cloneFlags.prepend("transient");
  }
  cloneFlags.removeAll("forking");
  properties["flags"] = cloneFlags;
  QDBusObjectPath clonePath = apps.registerApplication(properties);
  if (clonePath.path() == "/") {
    qDebug() << "Failed to register transient instance";
    return EXIT_FAILURE;
  }
  Application clone(OXIDE_SERVICE, clonePath.path(), bus);
  clone.launchWithArgs(launchArgs).waitForFinished();
  return EXIT_SUCCESS;
}