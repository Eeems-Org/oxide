#pragma once

#include <liboxide.h>
#include <signal.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QTimer>

#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;
using namespace Oxide;
using namespace Oxide::Sentry;

class Controller : public QObject {
  Q_OBJECT

public:
  explicit Controller(QObject* parent = 0)
    : QObject(parent) {
    auto bus = QDBusConnection::systemBus();
    qDebug() << "Waiting for tarnish to start up";
    int retries = 0;
    const int maxRetries = 300;
    while (!bus.interface()->registeredServiceNames().value().contains(
      OXIDE_SERVICE
    )) {
      if (++retries >= maxRetries) {
        qWarning() << "Timed out waiting for tarnish to start up";
        return;
      }
      struct timespec args{
        .tv_sec = 1,
        .tv_nsec = 0,
      },
        res;
      nanosleep(&args, &res);
    }
    qDebug() << "Requesting General API";
    api = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    connect(api, &General::aboutToQuit, qApp, &QGuiApplication::quit);

    // Setup signal handlers for backgroundable support
    SignalHandler::setup_unix_signal_handlers();
    signalHandler->removeNotifier(SIGTERM);
    signalHandler->removeNotifier(SIGINT);
    signalHandler->removeNotifier(SIGCONT);
    signalHandler->removeNotifier(SIGPIPE);
    signalHandler->removeNotifier(SIGSEGV);
    signalHandler->removeNotifier(SIGBUS);
    connect(signalHandler, &SignalHandler::sigUsr1, this, &Controller::sigUsr1);
    connect(signalHandler, &SignalHandler::sigUsr2, this, &Controller::sigUsr2);
  }

  Q_INVOKABLE void
  breadcrumb(QString category, QString message, QString type = "default") {
#ifdef SENTRY
    sentry_breadcrumb(
      category.toStdString().c_str(),
      message.toStdString().c_str(),
      type.toStdString().c_str()
    );
#else
    Q_UNUSED(category);
    Q_UNUSED(message);
    Q_UNUSED(type);
#endif
  }

  // === Category navigation ===
  Q_INVOKABLE void navigateToCategory(QString name) {
    emit categoryRequested(name);
  }

  // Expose General API for category controllers
  General* generalApi() { return api; }

signals:
  void categoryRequested(QString name);

private slots:
  void sigUsr1() {
    ::kill(api->tarnishPid(), SIGUSR1);
    qDebug() << "Settings foregrounded";
  }
  void sigUsr2() {
    qDebug() << "Settings backgrounded";
    QTimer::singleShot(0, [this] { ::kill(api->tarnishPid(), SIGUSR2); });
  }

private:
  General* api = nullptr;
};
