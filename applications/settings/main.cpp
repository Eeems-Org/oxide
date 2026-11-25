#include <cstdlib>
#include <liboxide.h>
#include <liboxide/oxideqml.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <signal.h>

#include "controller.h"
#include "displaycontroller.h"
#include "generalcontroller.h"
#include "gesturescontroller.h"
#include "notificationscontroller.h"
#include "powercontroller.h"
#include "wificontroller.h"

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

int
qExit(int ret) {
  QTimer::singleShot(0, [ret]() { qApp->exit(ret); });
  return qApp->exec();
}

int
main(int argc, char* argv[]) {
  deviceSettings.setupQtEnvironment();
  QGuiApplication app(argc, argv);
  sentry_init("codes.eeems.settings", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("codes.eeems.settings");
  app.setApplicationDisplayName("Settings");
  app.setApplicationVersion(APP_VERSION);

  QCommandLineParser parser;
  parser.setApplicationDescription("Oxide Settings");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("category", "Settings category to open");
  parser.process(app);

  QString requestedCategory;
  if (!parser.positionalArguments().isEmpty()) {
    requestedCategory = parser.positionalArguments().first();
  }

  if (!ipc->start()) {
    return ipc->send(
             requestedCategory.isEmpty() ? "\n" : requestedCategory.toUtf8()
           )
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
  }

  auto bus = QDBusConnection::systemBus();
  O_DEBUG("Waiting for tarnish to start up");
  int retries = 0;
  const int maxRetries = 300;
  while (!bus.interface()->registeredServiceNames().value().contains(
    OXIDE_SERVICE
  )) {
    if (++retries >= maxRetries) {
      O_WARNING("Timed out waiting for tarnish to start up");
      return qExit(EXIT_FAILURE);
    }
    struct timespec args{
      .tv_sec = 1,
      .tv_nsec = 0,
    },
      res;
    nanosleep(&args, &res);
  }
  O_DEBUG("Requesting General API");
  auto* generalApi = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  QObject::connect(
    generalApi, &General::aboutToQuit, &app, &QGuiApplication::quit
  );
#define initApi(type, name)                                                    \
  type* name##Api;                                                             \
  {                                                                            \
    O_DEBUG("Requesting " #name " API");                                       \
    auto reply = generalApi->requestAPI(#name);                                \
    reply.waitForFinished();                                                   \
    if (reply.isError()) {                                                     \
      O_WARNING("Unable to get " #name " API:" << reply.error().message());    \
      return qExit(EXIT_FAILURE);                                              \
    }                                                                          \
    auto path = reply.value().path();                                          \
    if (path == "/") {                                                         \
      O_WARNING("Unable to get " #name "API");                                 \
      return qExit(EXIT_FAILURE);                                              \
    }                                                                          \
    name##Api =                                                                \
      new type(OXIDE_SERVICE, path, QDBusConnection::systemBus(), &app);       \
  }
  initApi(Apps, apps);
  initApi(Notifications, notification);
  initApi(Frontlight, frontlight);
  initApi(System, system);
  initApi(Wifi, wifi);
#undef initApi

  Controller controller(generalApi, &app);
  GeneralController generalController(&app);
  DisplayController displayController(&app, notificationApi, frontlightApi);
  PowerController powerController(&app, systemApi);
  GesturesController gesturesController(&app, systemApi);
  WifiController wifiController(&app, wifiApi);
  NotificationsController notificationsController(&app, notificationApi);

  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();

  context->setContextProperty("controller", &controller);
  context->setContextProperty("generalController", &generalController);
  context->setContextProperty("displayController", &displayController);
  context->setContextProperty("powerController", &powerController);
  context->setContextProperty("gesturesController", &gesturesController);
  context->setContextProperty("wifiController", &wifiController);
  context->setContextProperty(
    "notificationsController", &notificationsController
  );
  QTimer::singleShot(
    0, [&app, &engine, &controller, &requestedCategory, appsApi] {
      auto* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
      if (root == nullptr) {
        qDebug() << "Nothing to display";
        qApp->exit(EXIT_FAILURE);
        return;
      }
      root->installEventFilter(new EventFilter(&app));
      QObject::connect(
        ipc, &IPC::message, [&controller, appsApi](const QByteArray& data) {
          QString category = QString::fromUtf8(data).trimmed();
          qDebug() << "Recieved" << category;
          if (!category.isEmpty()) {
            controller.navigateToCategory(category);
          }
          auto reply = appsApi->application();
          if (reply.isError()) {
            qDebug() << "Unable to get current application path: "
                     << reply.error().message();
            emit controller.foregrounded();
            return;
          }
          auto path = reply.value();
          if (path.path() == "/") {
            qDebug() << "Unable to get current application path";
            emit controller.foregrounded();
            return;
          }
          qDebug() << "Resuming";
          Application app(
            OXIDE_SERVICE, path.path(), QDBusConnection::systemBus()
          );
          app.resume();
        }
      );
      if (!requestedCategory.isEmpty()) {
        controller.navigateToCategory(requestedCategory);
      }
    }
  );
  return app.exec();
}
