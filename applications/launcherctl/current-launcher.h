#pragma once

#include "common.h"

class CurrentLauncherCommand : ICommand {
  O_COMMAND(
    CurrentLauncherCommand,
    "current-launcher",
    "Get the name of the current launcher",
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
    qStdOut() << currentLauncher() << "\n";
    return EXIT_SUCCESS;
  }
};
