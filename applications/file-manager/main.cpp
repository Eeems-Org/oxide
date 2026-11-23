#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>
#include <signal.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <cstdlib>

#include "controller.h"

using namespace std;
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
  sentry_init("defile", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("defile");
  app.setApplicationVersion(APP_VERSION);

  Controller controller(&app);
  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty("controller", &controller);

  signal(SIGINT, sigHandler);
  signal(SIGSEGV, sigHandler);
  signal(SIGTERM, sigHandler);

  QTimer::singleShot(0, [&app, &engine, &controller] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    root->installEventFilter(new Oxide::EventFilter(&app));
  });
  return app.exec();
}
