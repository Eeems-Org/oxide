#pragma once

#include "common.h"

class ListAppsCommand : ICommand {
  O_COMMAND(
    ListAppsCommand,
    "list-apps",
    "List all applications for the current launcher",
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
    queryCurrent({"apps"}, &output);
    qStdOut() << output;
    return EXIT_SUCCESS;
  }
};
