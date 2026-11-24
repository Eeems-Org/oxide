#pragma once
#include "common.h"

#include <liboxide.h>
#include <liboxide/applications.h>

#include <QDBusConnection>
#include <QDebug>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

class DefaultCommand : ICommand {
  O_COMMAND(
    DefaultCommand,
    "default",
    "Set default application for mimetype(s)"
  )
  int arguments() override {
    parser->addPositionalArgument("application", "Application name");
    parser->addPositionalArgument(
      "mimetypes", "MIME type(s)", "[MIME_TYPE...]"
    );
    return EXIT_SUCCESS;
  }
  int command(const QStringList& args) override {
    if (args.size() < 2) {
      qDebug() << "Usage: xdg-mime default application mimetype(s)";
      return EXIT_FAILURE;
    }

    auto bus = QDBusConnection::systemBus();
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    QDBusObjectPath path = api.requestAPI("apps");
    if (path.path() == "/") {
      qDebug() << "Unable to get apps API";
      return EXIT_FAILURE;
    }
    Apps apps(OXIDE_SERVICE, path.path(), bus);

    QString appName = args[0];
    QStringList mimeTypes = args.mid(1);
    QDBusObjectPath appPath = apps.getApplicationPath(appName);
    if (appPath.path() == "/") {
      qDebug() << "Application does not exist:" << appName;
      return EXIT_FAILURE;
    }
    for (const QString& mimeType : mimeTypes) {
      apps.setDefaultApplication(mimeType, appPath);
      qDebug() << "Set default for" << mimeType << "to" << appName;
    }
    return EXIT_SUCCESS;
  }
};
