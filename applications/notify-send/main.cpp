#include <csignal>
#include <cstdlib>
#include <liboxide.h>

#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMutableListIterator>
#include <QSet>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide::JSON;

int
qExit(int ret) {
  QTimer::singleShot(0, [ret]() { qApp->exit(ret); });
  return qApp->exec();
}

int
main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  sentry_init("notify-send", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("notify-send");
  app.setApplicationVersion(APP_VERSION);
  QCommandLineParser parser;
  parser.setApplicationDescription("a program to send desktop notifications");
  parser.applicationDescription();
  parser.addVersionOption();
  parser.addPositionalArgument(
    "summary", "Summary of the notification", "{SUMMARY}"
  );
  parser.addPositionalArgument("body", "Body of the notification", "[BODY]");
  QCommandLineOption appNameOption(
    {"a", "app-name"},
    "Specifies the app name for the notification.",
    "APP_NAME"
  );
  parser.addOption(appNameOption);
  QCommandLineOption iconOption(
    {"i", "icon"},
    "Specifies an icon filename or stock icon to display.",
    "ICON"
  );
  parser.addOption(iconOption);
  QCommandLineOption expireOption(
    {"t", "expire-time"},
    "The duration, in milliseconds to wait for the notification. Does "
    "nothing without --wait",
    "TIME"
  );
  parser.addOption(expireOption);
  QCommandLineOption printOption(
    {"p", "print-id"}, "Print the notification identifier."
  );
  parser.addOption(printOption);
  QCommandLineOption replaceOption(
    {"r", "replace-id"},
    "The identifier of the notification to replace.",
    "REPLACE_ID"
  );
  parser.addOption(replaceOption);
  QCommandLineOption waitOption(
    {"w", "wait"},
    "Wait for the notification to be closed before exiting. If the "
    "expire-time is set, it will be used as the maximum waiting time."
  );
  parser.addOption(waitOption);
  QCommandLineOption transientOption(
    {"e", "transient"},
    "Show a transient notification. This notification will be removed "
    "immediately after being shown on the screen"
  );
  parser.addOption(transientOption);
  QCommandLineOption urgencyOption(
    {"u", "urgency"}, "NOT IMPLEMENTED", "LEVEL"
  );
  parser.addOption(urgencyOption);
  QCommandLineOption appOption(
    {"A", "action"},
    "Specifies the actions to display to the user. Implies --wait to wait for "
    "user input. May be set multiple times. The NAME of the action is output "
    "to stdout. If NAME is not specified, the numerical index of the option is "
    "used (starting with 0).",
    "NAME=TEXT"
  );
  parser.addOption(appOption);
  QCommandLineOption selectedActionFdOption(
    "selected-action-fd", "NOT IMPLEMENTED", "FILE_DESCRIPTOR"
  );
  parser.addOption(selectedActionFdOption);
  QCommandLineOption activationTokenFdOption(
    "activation-token-fd", "NOT IMPLEMENTED", "FILE_DESCRIPTOR"
  );
  parser.addOption(activationTokenFdOption);
  QCommandLineOption idFdOption("id-fd", "NOT IMPLEMENTED", "FILE_DESCRIPTOR");
  parser.addOption(idFdOption);
  QCommandLineOption categoryOption(
    {"c", "category"}, "NOT IMPLEMENTED", "TYPE[,TYPE]"
  );
  parser.addOption(categoryOption);
  QCommandLineOption hintOption(
    {"h", "hint"}, "NOT IMPLEMENTED", "TYPE:NAME:VALUE"
  );
  parser.addOption(hintOption);
  QCommandLineOption helpOption({"?", "help"}, "Show help and exit");
  parser.addOption(helpOption);
  parser.process(app);
  if (parser.isSet(helpOption)) {
    parser.showHelp();
  }
  QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
#ifdef SENTRY
    sentry_breadcrumb("error", "No arguments");
#endif
    parser.showHelp(EXIT_FAILURE);
  }
  if (args.count() > 2) {
#ifdef SENTRY
    sentry_breadcrumb("error", "Too many arguments");
#endif
    parser.showHelp(EXIT_FAILURE);
  }
  auto bus = QDBusConnection::systemBus();
  General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  qDebug() << "Requesting notification API...";
  QDBusObjectPath path = api.requestAPI("notification");
  if (path.path() == "/") {
    qDebug() << "Unable to get notification API";
    return qExit(EXIT_FAILURE);
  }
  Notifications notifications(OXIDE_SERVICE, path.path(), bus);
  QString guid;
  auto appName = parser.isSet(appNameOption) ? parser.value(appNameOption)
                                             : "codes.eeems.notify-send";
  auto iconPath = parser.isSet(iconOption) ? parser.value(iconOption) : "";
  auto text = args.join("\n");
  if (parser.isSet(replaceOption)) {
    guid = parser.value(replaceOption);
    path = notifications.get(guid);
    if (path.path() == "/" || !notifications.take(guid)) {
      guid = QUuid::createUuid().toString();
      path = notifications.add(guid, appName, text, iconPath);
    }
    if (path.path() == "/") {
      qDebug() << "Failed to add notification";
      return qExit(EXIT_FAILURE);
    }
    Notification notification(OXIDE_SERVICE, path.path(), bus);
    notification.setApplication(appName);
    notification.setIcon(iconPath);
    notification.setText(text);
  } else {
    guid = QUuid::createUuid().toString();
    path = notifications.add(guid, appName, text, iconPath);
    if (path.path() == "/") {
      qDebug() << "Failed to add notification";
      return qExit(EXIT_FAILURE);
    }
  }
  Notification notification(OXIDE_SERVICE, path.path(), bus);
  QVariantMap actionMap;
  unsigned int optionIndex = 0;
  for (const auto& a : parser.values(appOption)) {
    int eq = a.indexOf('=');
    if (eq == -1) {
      actionMap[QString::number(optionIndex)] = a;
    } else if (eq == 0) {
      actionMap[QString::number(optionIndex)] = a.mid(eq + 1);
    } else {
      actionMap[a.left(eq)] = a.mid(eq + 1);
    }
    optionIndex++;
  }
  if (!actionMap.isEmpty()) {
    notification.setActions(actionMap);
  }
  if (parser.isSet(printOption)) {
    QTextStream qStdOut(stdout, QIODevice::WriteOnly);
    qStdOut << guid << Qt::endl;
  }
  if (!parser.isSet(waitOption) && actionMap.isEmpty()) {
    qDebug() << "Displaying notification" << guid;
    notification.display().waitForFinished();
    if (parser.isSet(transientOption)) {
      notification.remove();
    }
    return qExit(EXIT_SUCCESS);
  }
  unsigned int timeout = 0;
  if (parser.isSet(expireOption)) {
    bool ok = false;
    timeout = parser.value(expireOption).toUInt(&ok);
    if (!ok) {
      qDebug() << "Invalid --expire-time value";
      return qExit(EXIT_FAILURE);
    }
  }
  Oxide::SignalHandler::setup_unix_signal_handlers();
  QObject::connect(
    signalHandler, &Oxide::SignalHandler::sigTerm, [&notification] {
      notification.remove().waitForFinished();
      qApp->exit(128 + SIGTERM);
    }
  );
  QObject::connect(
    signalHandler, &Oxide::SignalHandler::sigInt, [&notification] {
      notification.remove().waitForFinished();
      qApp->exit(128 + SIGINT);
    }
  );
  QObject::connect(
    signalHandler, &Oxide::SignalHandler::sigSegv, [&notification] {
      notification.remove().waitForFinished();
      qApp->exit(128 + SIGSEGV);
    }
  );
  QObject::connect(
    signalHandler, &Oxide::SignalHandler::sigBus, [&notification] {
      notification.remove().waitForFinished();
      qApp->exit(128 + SIGBUS);
    }
  );
  if (!Oxide::DBusConnect(
        &notification,
        "displayed",
        [&notification, timeout](QVariantList args) {
          Q_UNUSED(args);
          qDebug() << "Waiting for notification to be closed";
          if (!Oxide::DBusConnect(
                &notification,
                "removed",
                [](QVariantList args) {
                  Q_UNUSED(args);
                  qApp->exit(EXIT_SUCCESS);
                },
                true
              )) {
            qDebug() << "Failed to wait for notification to exit";
            qExit(EXIT_FAILURE);
          }
          if (!Oxide::DBusConnect(
                &notification, "clicked", [&notification](QVariantList args) {
                  notification.remove();
                  QString action = args.value(0).toString();
                  qDebug() << "clicked" << action;
                  if (!action.isEmpty()) {
                    QTextStream qStdOut(stdout, QIODevice::WriteOnly);
                    qStdOut << action << Qt::endl;
                  }
                  qApp->exit(EXIT_SUCCESS);
                }
              )) {
            qDebug() << "Failed to connect Notification::clicked";
            qExit(EXIT_FAILURE);
          }
          if (timeout) {
            qDebug()
              << ("Timeout set to " + std::to_string(timeout) + "ms").c_str();
            QTimer::singleShot(timeout, [&notification] {
              qDebug() << "Notification wait timed out";
              notification.remove().waitForFinished();
              qApp->exit(EXIT_SUCCESS);
            });
          }
        }
      )) {
    qDebug() << "Failed to connect Notification::displayed";
    return qExit(EXIT_FAILURE);
  }
  qDebug() << "Displaying notification" << guid;
  notification.display().waitForFinished();
  qDebug() << "Waiting for notification to be be displayed";
  return app.exec();
}
