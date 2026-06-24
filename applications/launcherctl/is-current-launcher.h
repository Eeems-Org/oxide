#pragma once

#include "common.h"

class IsCurrentLauncherCommand : ICommand {
  O_COMMAND(
    IsCurrentLauncherCommand,
    "is-current-launcher",
    "Check if the launcher is the current launcher"
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addOption(quietOption);
    parser.addPositionalArgument("launcher", "The launcher to check");
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto launcher = args.first();
    auto current = currentLauncher();
    bool quiet = parser.isSet("quiet");
    if (current == launcher) {
      if (!quiet) {
        qStdOut() << launcher << " is the current launcher\n";
      }
      return EXIT_SUCCESS;
    }
    if (!quiet) {
      qStdOut() << launcher << " is not the current launcher\n";
    }
    return EXIT_FAILURE;
  }

private:
  QCommandLineOption quietOption =
    QCommandLineOption("quiet", "Only return exit code");
};
