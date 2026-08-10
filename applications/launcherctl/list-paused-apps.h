#pragma once

#include "common.h"
#include <cstdlib>

class ListPausedAppsCommand : ICommand {
  O_COMMAND(
    ListPausedAppsCommand,
    "list-paused-apps",
    "List all paused applications for the current launcher",
    true
  )
  int arguments(QCommandLineParser& parser) override {
    Q_UNUSED(parser);
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (!args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    QString output;
    bool success = queryActive({"paused"}, &output);
    qStdOut() << output;
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
  }
};
