#include <cstdlib>
#include <liboxide.h>
#include <liboxide/oxideqml.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
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

#define LOCK_PATH "/run/oxide/codes.eeems.settings.lock"
#define SOCKET_PATH "/var/run/oxide/codes.eeems.settings.socket"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

static int lockFd = -1;
static QLocalServer* server = nullptr;

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

  lockFd = tryGetLock(LOCK_PATH);
  if (lockFd < 0) {
    bool receivedAck = false;
    QLocalSocket socket;
    socket.connectToServer(SOCKET_PATH);
    if (socket.waitForConnected(2000)) {
      socket.write(
        requestedCategory.isEmpty() ? "\n" : requestedCategory.toUtf8()
      );
      socket.flush();
      if (socket.waitForReadyRead(2000)) {
        socket.readAll();
        receivedAck = true;
      }
      socket.disconnectFromServer();
    }
    if (receivedAck) {
      return EXIT_SUCCESS;
    }
    qWarning() << "Settings is already running and did not acknowledge";
    return EXIT_FAILURE;
  }
  // Clean up lock on any exit from here on out
  QObject::connect(&app, &QGuiApplication::aboutToQuit, []() {
    if (server != nullptr) {
      server->close();
      QFile::remove(SOCKET_PATH);
    }
    releaseLock(lockFd, LOCK_PATH);
  });
  QFile::remove(SOCKET_PATH);
  server = new QLocalServer(&app);
  if (!server->listen(SOCKET_PATH)) {
    qWarning() << "Unable to start local server:" << server->errorString();
  }
  auto bus = QDBusConnection::systemBus();
  qDebug() << "Waiting for tarnish to start up";
  int retries = 0;
  const int maxRetries = 300;
  while (!bus.interface()->registeredServiceNames().value().contains(
    OXIDE_SERVICE
  )) {
    if (++retries >= maxRetries) {
      qWarning() << "Timed out waiting for tarnish to start up";
      return qExit(EXIT_FAILURE);
    }
    struct timespec args{
      .tv_sec = 1,
      .tv_nsec = 0,
    },
      res;
    nanosleep(&args, &res);
  }
  qDebug() << "Requesting General API";
  auto* generalApi = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  QObject::connect(
    generalApi, &General::aboutToQuit, &app, &QGuiApplication::quit
  );
#define initApi(type, name)                                                    \
  type* name##Api;                                                             \
  {                                                                            \
    auto reply = generalApi->requestAPI(#name);                                \
    reply.waitForFinished();                                                   \
    if (reply.isError()) {                                                     \
      qWarning() << "Unable to get " #name " API:" << reply.error().message(); \
      return qExit(EXIT_FAILURE);                                              \
    }                                                                          \
    auto path = reply.value().path();                                          \
    if (path == "/") {                                                         \
      qWarning() << "Unable to get " #name "API";                              \
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
        server, &QLocalServer::newConnection, [&controller, appsApi]() {
          QLocalSocket* socket = server->nextPendingConnection();
          if (socket == nullptr) {
            return;
          }
          QObject::connect(
            socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater
          );
          QObject::connect(
            socket, &QLocalSocket::readyRead, [socket, &controller, appsApi]() {
              QByteArray data = socket->readAll();
              QString category = QString::fromUtf8(data).trimmed();
              if (!category.isEmpty()) {
                qDebug() << "Opening" << category;
                controller.navigateToCategory(category);
              }
              auto reply = appsApi->application();
              if (reply.isError()) {
                qDebug() << "Unable to get current application path: "
                         << reply.error().message();
                socket->write("ok");
                socket->flush();
                socket->disconnectFromServer();
                emit controller.foregrounded();
                return;
              }
              auto path = reply.value();
              if (path.path() == "/") {
                qDebug() << "Unable to get current application path";
                socket->write("ok");
                socket->flush();
                socket->disconnectFromServer();
                emit controller.foregrounded();
                return;
              }
              qDebug() << "Resuming";
              Application app(
                OXIDE_SERVICE, path.path(), QDBusConnection::systemBus()
              );
              app.resume();
              socket->write("ok");
              socket->flush();
              socket->disconnectFromServer();
            }
          );
        }
      );
      if (!requestedCategory.isEmpty()) {
        controller.navigateToCategory(requestedCategory);
      }
    }
  );
  return app.exec();
}
