#pragma once

#include "common.h"

class RestartLauncherCommand : ICommand {
  O_COMMAND(
    RestartLauncherCommand,
    "restart-launcher",
    "Restart the current launcher",
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
    ICommand::exec({"stop-launcher"});
    return ICommand::exec({"start-launcher"});
  }
};
