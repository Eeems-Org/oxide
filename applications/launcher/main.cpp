#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>
#include <signal.h>

#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QtQuick>
#include <cstdlib>

#include "controller.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

function<void(int)> shutdown_handler;
void
signalHandler2(int signal) {
  shutdown_handler(signal);
}

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("oxide", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("oxide");
  app.setApplicationDisplayName("Launcher");
  QQmlApplicationEngine engine;
  QQmlContext* context = engine.rootContext();
  Controller* controller = new Controller();
  controller->loadSettings();
  qmlRegisterAnonymousType<AppItem>("codes.eeems.oxide", 2);
  qmlRegisterAnonymousType<Controller>("codes.eeems.oxide", 2);
  registerQML(&engine);
  context->setContextProperty("controller", controller);
  QTimer::singleShot(0, [&app, &engine, controller] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    controller->root = root;
    root->installEventFilter(new EventFilter(&app));
    QObject* stateController = root->findChild<QObject*>("stateController");
    if (!stateController) {
      qDebug() << "Can't find stateController";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    Q_UNUSED(stateController);
  });
  shutdown_handler = [&controller](int signum) {
    Q_UNUSED(signum)
    QTimer::singleShot(300, [=]() { controller->refreshApps(); });
  };
  signal(SIGCONT, signalHandler2);
  return app.exec();
}
