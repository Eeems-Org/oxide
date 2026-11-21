#include <liboxide/devicesettings.h>
#include <liboxide/eventfilter.h>
#include <liboxide/meta.h>
#include <liboxide/oxideqml.h>
#include <liboxide/sentry.h>

#include "keyboardhandler.h"
#include "terminalview.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QtPlugin>
#include <QtQuick>

#include <QDebug>

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("wither", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("wither");
  app.setApplicationVersion(APP_VERSION);

  qmlRegisterType<TerminalView>("Terminal", 1, 0, "TerminalView");
  qmlRegisterType<KeyboardHandler>("KeyboardHandler", 1, 0, "KeyboardHandler");

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
  return app.exec();
}
