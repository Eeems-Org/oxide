#include <liboxide.h>
#include <liboxide/applications.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>
#include <signal.h>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUuid>
#include <QtQuick>

#include "controller.h"

using namespace Oxide;
using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace Oxide::QML;
using namespace codes::eeems::oxide1;

void
sigHandler(int signal) {
  ::signal(signal, SIG_DFL);
  qApp->quit();
}

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
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
    qDebug() << "Usage: openwith <mimetype> <filepath>";
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

  QList<QDBusObjectPath> candidates = apps.applicationsForMimeType(mimeType);
  if (candidates.isEmpty()) {
    qDebug() << "No applications found for MIME type:" << mimeType;
    return EXIT_FAILURE;
  }

  QVariantList candidateList;
  for (auto& c : candidates) {
    Application a(OXIDE_SERVICE, c.path(), bus);
    QVariantMap app;
    app["path"] = c.path();
    app["displayName"] = a.displayName();
    app["icon"] = a.icon();
    candidateList.append(app);
  }

  Controller controller(&apps);

  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty("candidates", candidateList);
  context->setContextProperty("mimeType", mimeType);
  context->setContextProperty("filePath", filePath);
  context->setContextProperty("chooser", &controller);

  signal(SIGINT, sigHandler);
  signal(SIGSEGV, sigHandler);
  signal(SIGTERM, sigHandler);

  QTimer::singleShot(0, [&app, &engine] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    root->installEventFilter(new EventFilter(&app));
  });
  return app.exec();
}
