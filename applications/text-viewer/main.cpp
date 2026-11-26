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

#include "lazylinemodel.h"

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
  sentry_init("codes.eeems.text", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("codes.eeems.text");
  app.setApplicationVersion(APP_VERSION);

  QCommandLineParser parser;
  parser.addPositionalArgument("file", "File to view");
  parser.addHelpOption();
  parser.process(app);

  QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
    qDebug() << "Usage: codes.eeems.text <file>";
    return EXIT_FAILURE;
  }

  QString filePath = args.first();

  LazyLineModel* model = new LazyLineModel(&app);
  model->setFilePath(filePath);

  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty("filePath", filePath);
  context->setContextProperty("lineModel", model);

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