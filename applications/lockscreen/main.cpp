#include <liboxide/dbus.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <cstdlib>

#include "controller.h"

using namespace codes::eeems::oxide1;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("decay", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("decay");
  app.setApplicationVersion(APP_VERSION);
  Controller controller(&app);
  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty("controller", &controller);
  QTimer::singleShot(0, [&app, &engine, &controller] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    root->installEventFilter(new EventFilter(&app));
    controller.setRoot(root);
    qDebug() << root;
  });
  return app.exec();
}
