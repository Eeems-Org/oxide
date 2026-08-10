#pragma once

#include "common.h"
#include <cstdlib>

class PauseAppCommand : ICommand {
  O_COMMAND(
    PauseAppCommand,
    "pause-app",
    "Pause an application with the current launcher"
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addPositionalArgument("application", "The application to pause");
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto app = args.first();
    checkLauncherHasApp(app);
    return queryActive({"pause", app}) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
};
