#pragma once

#include "common.h"

class ListRunningAppsCommand : ICommand {
  O_COMMAND(
    ListRunningAppsCommand,
    "list-running-apps",
    "List all running applications for the current launcher",
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
    queryActive({"running"}, &output);
    qStdOut() << output;
    return EXIT_SUCCESS;
  }
};
