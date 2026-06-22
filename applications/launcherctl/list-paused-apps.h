#pragma once

#include "common.h"

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
    queryActive({"paused"}, &output);
    qStdOut() << output;
    return EXIT_SUCCESS;
  }
};
