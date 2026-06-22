#pragma once

#include "common.h"

class StartAppCommand : ICommand {
  O_COMMAND(
    StartAppCommand,
    "start-app",
    "Start an application with the current launcher"
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addPositionalArgument("application", "The application to start");
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto app = args.first();
    checkLauncherHasApp(app);
    queryActive({"launch", app});
    return EXIT_SUCCESS;
  }
};
