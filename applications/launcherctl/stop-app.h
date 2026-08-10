#pragma once

#include "common.h"
#include <cstdlib>

class StopAppCommand : ICommand {
  O_COMMAND(
    StopAppCommand,
    "stop-app",
    "Stop an application with the current launcher"
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addPositionalArgument("application", "The application to stop");
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto app = args.first();
    checkLauncherHasApp(app);
    if (!queryActive({"close", app})) {
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
};
