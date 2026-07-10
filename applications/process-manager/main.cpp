#include <cstdlib>
#include <fcntl.h>
#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>
#include <linux/input.h>
#include <signal.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <ostream>

#include "controller.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("erode", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("erode");
  app.setApplicationDisplayName("Process Monitor");
  app.setApplicationVersion(APP_VERSION);
  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  Controller controller(&engine);
  context->setContextProperty("controller", &controller);
  QTimer::singleShot(0, [&app, &engine] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    root->installEventFilter(new EventFilter(&app));
    QQuickItem* tasksView = root->findChild<QQuickItem*>("tasksView");
    if (!tasksView) {
      qDebug() << "Can't find tasksView";
      qApp->exit(EXIT_FAILURE);
      return;
    }
  });
  return app.exec();
}
