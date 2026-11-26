#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>
#include <signal.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuick>
#include <cstdlib>

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

void
sigHandler(int signal) {
  ::signal(signal, SIG_DFL);
  qApp->quit();
}

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("codes.eeems.image", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("codes.eeems.image");
  app.setApplicationVersion(APP_VERSION);

  QCommandLineParser parser;
  parser.addPositionalArgument("file", "Image file to view");
  parser.addHelpOption();
  parser.process(app);

  QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
    qDebug() << "Usage: codes.eeems.image <file>";
    return EXIT_FAILURE;
  }

  QString filePath = args.first();

  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty("filePath", filePath);

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