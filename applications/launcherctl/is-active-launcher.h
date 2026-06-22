#pragma once

#include "common.h"

class IsActiveLauncherCommand : ICommand {
  O_COMMAND(
    IsActiveLauncherCommand,
    "is-active-launcher",
    "Check if the launcher is active"
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
    bool quiet = parser.isSet("quiet");
    if (query(launcher, {"is-active"})) {
      if (!quiet) {
        qStdOut() << "active\n";
      }
      return EXIT_SUCCESS;
    }
    if (!quiet) {
      qStdOut() << "inactive\n";
    }
    return EXIT_FAILURE;
  }

private:
  QCommandLineOption quietOption =
    QCommandLineOption("quiet", "Only return exit code");
};
