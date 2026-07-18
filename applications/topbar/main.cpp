#include <cstring>
#include <libblight/system.h>
#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>
#include <signal.h>
#include <unistd.h>

#include <QGuiApplication>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QStringList>
#include <QtDBus>
#include <QtPlugin>
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
  sentry_init("stain", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("stain");
  app.setApplicationDisplayName("Topbar");
  QQmlApplicationEngine engine;
  QQmlContext* context = engine.rootContext();
  Controller* controller = new Controller();
  controller->loadSettings();
  qmlRegisterAnonymousType<Controller>("codes.eeems.stain", 3);
  registerQML(&engine);
  context->setContextProperty("controller", controller);
  QTimer::singleShot(0, [&app, &engine, controller] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    auto window = qobject_cast<QWindow*>(root);
    if (window == nullptr) {
      qDebug() << "Root is not a QWindow";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    auto buffer = Oxide::QML::getSurfaceForWindow(window);
    if (buffer == nullptr) {
      qDebug() << "Unable to get buffer for window";
      qApp->exit(EXIT_FAILURE);
    }
    if (!Blight::setFlags(
          QString("connection/%1/surface/%2")
            .arg(getpid())
            .arg(buffer->surface)
            .toStdString(),
          {"system"}
        )) {
      O_WARNING("Unable to set flags on surface" << std::strerror(errno));
    }
    controller->root = root;
    root->installEventFilter(new EventFilter(&app));
    QObject* clock = root->findChild<QObject*>("clock");
    if (!clock) {
      qDebug() << "Can't find clock";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    // Update UI
    clock->setProperty("text", QTime::currentTime().toString("h:mm a"));
    // Setup clock
    QTimer* clockTimer = new QTimer(root);
    auto currentTime = QTime::currentTime();
    QTime nextTime = currentTime.addSecs(60 - currentTime.second());
    clockTimer->setInterval(currentTime.msecsTo(nextTime)); // nearest minute
    QObject::connect(
      clockTimer, &QTimer::timeout, [clock, clockTimer, controller]() {
        QString text = "";
        if (controller->showDate()) {
          text = QDate::currentDate().toString(Qt::TextDate) + " ";
        }
        clock->setProperty(
          "text", text + QTime::currentTime().toString("h:mm a")
        );
        if (clockTimer->interval() != 60 * 1000) {
          clockTimer->setInterval(60 * 1000); // 1 minute
        }
      }
    );
    clockTimer->start();
  });
  return app.exec();
}
