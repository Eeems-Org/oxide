#pragma once

#include "common.h"

class ListLaunchersCommand : ICommand {
  O_COMMAND(
    ListLaunchersCommand,
    "list-launchers",
    "List installed launchers",
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
    for (const auto& launcher : launchers()) {
      qStdOut() << launcher << "\n";
    }
    return EXIT_SUCCESS;
  }
};
