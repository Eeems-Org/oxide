#include <liboxide.h>
#include <liboxide/oxideqml.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QThread>
#include <signal.h>

#include "controller.h"

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

#define LOCK_PATH "/run/oxide/codes.eeems.settings.lock"
#define SOCKET_PATH "/var/run/oxide/codes.eeems.settings.socket"

static int lockFd = -1;
static QLocalServer* server = nullptr;

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

  // Single instance: try flock
  lockFd = tryGetLock(LOCK_PATH);
  if (lockFd < 0) {
    // Another instance may be running — try socket handshake
    bool receivedAck = false;
    QLocalSocket socket;
    socket.connectToServer(SOCKET_PATH);
    if (socket.waitForConnected(2000)) {
      socket.write(requestedCategory.toUtf8());
      socket.flush();
      // Wait for ack from the running instance
      if (socket.waitForReadyRead(2000)) {
        socket.readAll();
        receivedAck = true;
      }
      socket.disconnectFromServer();
    }
    if (receivedAck) {
      // Running instance acknowledged, category forwarded
      return 0;
    }
    // No ack — assume dead, kill holder and continue as primary
    auto pids = lsof(LOCK_PATH);
    for (int pid : pids) {
      if (pid > 0) {
        kill(pid, SIGTERM);
      }
    }
    // Wait briefly for process to die and lock to release
    QThread::msleep(200);
    QFile::remove(SOCKET_PATH);
    QFile::remove(LOCK_PATH);
    lockFd = tryGetLock(LOCK_PATH);
    if (lockFd < 0) {
      qWarning() << "Unable to acquire lock after killing stale process";
      return EXIT_FAILURE;
    }
  }

  // Clean up stale socket file if it exists (previous crash)
  QFile::remove(SOCKET_PATH);

  // Set up local server for single-instance IPC
  server = new QLocalServer(&app);
  if (!server->listen(SOCKET_PATH)) {
    qWarning() << "Unable to start local server:" << server->errorString();
  }

  Controller controller(&app);

  // Connect new connection signal to category navigation
  QObject::connect(server, &QLocalServer::newConnection, [&controller]() {
    QLocalSocket* socket = server->nextPendingConnection();
    if (socket == nullptr) {
      return;
    }
    QObject::connect(socket, &QLocalSocket::readyRead, [socket, &controller]() {
      QByteArray data = socket->readAll();
      socket->write("ok");
      socket->flush();
      socket->disconnectFromServer();
      QString category = QString::fromUtf8(data);
      if (!category.isEmpty()) {
        controller.navigateToCategory(category);
      }
    });
  });

  QQmlApplicationEngine engine;
  registerQML(&engine);
  QQmlContext* context = engine.rootContext();
  context->setContextProperty("controller", &controller);

  QTimer::singleShot(0, [&app, &engine, &controller, &requestedCategory] {
    QObject* root = loadQML(&engine, QUrl(QStringLiteral("qrc:/main.qml")));
    if (root == nullptr) {
      qDebug() << "Nothing to display";
      qApp->exit(EXIT_FAILURE);
      return;
    }
    root->installEventFilter(new EventFilter(&app));
    controller.setRoot(root);
    if (!requestedCategory.isEmpty()) {
      controller.navigateToCategory(requestedCategory);
    }
  });

  // Clean up on quit
  QObject::connect(&app, &QGuiApplication::aboutToQuit, []() {
    if (server != nullptr) {
      server->close();
      QFile::remove(SOCKET_PATH);
    }
    releaseLock(lockFd, LOCK_PATH);
  });

  return app.exec();
}
