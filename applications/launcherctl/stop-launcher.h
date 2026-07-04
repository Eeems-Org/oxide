#pragma once

#include "common.h"

class StopLauncherCommand : ICommand {
  O_COMMAND(
    StopLauncherCommand,
    "stop-launcher",
    "Stop the active launcher",
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
    queryActive({"stop"});
    return EXIT_SUCCESS;
  }
};
