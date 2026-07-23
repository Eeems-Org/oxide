#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>

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
    controller->setRoot(root);
    root->installEventFilter(new EventFilter(&app));
  });
  signalHandler->removeNotifier(SIGTERM);
  signalHandler->removeNotifier(SIGINT);
  signalHandler->removeNotifier(SIGUSR1);
  signalHandler->removeNotifier(SIGUSR2);
  signalHandler->removeNotifier(SIGPIPE);
  signalHandler->removeNotifier(SIGSEGV);
  signalHandler->removeNotifier(SIGBUS);
  QObject::connect(signalHandler, &SignalHandler::sigCont, [controller]() {
    QTimer::singleShot(300, controller, [controller]() {
      controller->refreshApps();
    });
  });
  return app.exec();
}
