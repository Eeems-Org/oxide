#pragma once

#include <liboxide.h>
#include <signal.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QTimer>

#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;
using namespace Oxide;
using namespace Oxide::Sentry;

class Controller : public QObject {
  Q_OBJECT

public:
  explicit Controller(General* api, QObject* parent = 0)
    : QObject(parent)
    , api(api) {
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

  Q_INVOKABLE void navigateToCategory(QString name) {
    emit categoryRequested(name);
  }

signals:
  void categoryRequested(QString name);
  void backgrounded();
  void foregrounded();

private slots:
  void sigUsr1() {
    qDebug() << "Settings foregrounded";
    emit foregrounded();
    ::kill(api->tarnishPid(), SIGUSR1);
  }
  void sigUsr2() {
    qDebug() << "Settings backgrounded";
    emit backgrounded();
    ::kill(api->tarnishPid(), SIGUSR2);
  }

private:
  General* api = nullptr;
};
