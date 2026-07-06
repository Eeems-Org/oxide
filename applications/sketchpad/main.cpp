#include <liboxide/devicesettings.h>
#include <liboxide/eventfilter.h>
#include <liboxide/meta.h>
#include <liboxide/oxideqml.h>
#include <liboxide/sentry.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("ether", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("ether");
  app.setApplicationVersion(APP_VERSION);
  QQmlApplicationEngine engine;
  registerQML(&engine);
  QTimer::singleShot(0, [&app, &engine] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    root->installEventFilter(new EventFilter(&app));
    qDebug() << root;
  });
  app.setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, false);
  return app.exec();
}
