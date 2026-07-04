#pragma once

#include "common.h"

class KillLauncherCommand : ICommand {
  O_COMMAND(
    KillLauncherCommand,
    "kill-launcher",
    "Force stop the active launcher",
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
    queryActive({"kill"});
    return EXIT_SUCCESS;
  }
};
