#pragma once
#include "common.h"

#include <liboxide.h>
#include <liboxide/applications.h>

#include <QDBusConnection>
#include <QDebug>
#include <QFileInfo>
#include <QMimeDatabase>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

class QueryCommand : ICommand {
  O_COMMAND(QueryCommand, "query", "Query information about file type handling")
  int arguments() override {
    parser->addPositionalArgument(
      "subcommand",
      "Query filetype or get the default application for a mimetype",
      "{ filetype FILE |"
    );
    parser->addPositionalArgument(
      "FILE | mimetype", "File or mimetype to query", "default mimetype }"
    );
    return EXIT_SUCCESS;
  }
  int command(const QStringList& args) override {
    if (args.size() != 2) {
      qDebug() << "Usage: xdg-mime query { filetype FILE | default mimetype }";
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

    if (args[0] == "filetype") {
      QFileInfo info(args[1]);
      if (!info.exists()) {
        qDebug() << "File does not exist:" << args[1];
        return EXIT_FAILURE;
      }
      qDebug()
        << QMimeDatabase().mimeTypeForFile(info).name().toStdString().c_str();
      return EXIT_SUCCESS;
    }
    if (args[0] == "default") {
      QDBusObjectPath appPath = apps.defaultApplication(args[1]);
      if (appPath.path() == "/") {
        qDebug() << "No default application for" << args[1];
        return EXIT_FAILURE;
      }
      qDebug() << Application(OXIDE_SERVICE, appPath.path(), bus)
                    .name()
                    .toStdString()
                    .c_str();
      return EXIT_SUCCESS;
    }
    qDebug() << "Unknown query subcommand:" << args[0];
    return EXIT_FAILURE;
  }
};
