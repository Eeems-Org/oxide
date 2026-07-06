#include <liboxide.h>
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
  qApp->exit(signal);
}

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("corrupt", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("corrupt");
  app.setApplicationVersion(APP_VERSION);
  Controller controller(&app);
  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty(
    "apps", QVariant::fromValue(controller.getApps())
  );
  context->setContextProperty("controller", &controller);

  signal(SIGINT, sigHandler);
  signal(SIGSEGV, sigHandler);
  signal(SIGTERM, sigHandler);

  QTimer::singleShot(0, [&engine, &controller] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    controller.setRoot(root);
  });
  return app.exec();
}
