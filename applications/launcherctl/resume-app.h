#pragma once

#include "common.h"
#include <cstdlib>

class ResumeAppCommand : ICommand {
  O_COMMAND(
    ResumeAppCommand,
    "resume-app",
    "Resume an application with the current launcher"
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addPositionalArgument("application", "The application to resume");
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto app = args.first();
    checkLauncherHasApp(app);
    if (!queryActive({"resume", app})) {
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
};
