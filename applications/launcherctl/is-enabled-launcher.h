#pragma once

#include "common.h"

class IsEnabledLauncherCommand : ICommand {
  O_COMMAND(
    IsEnabledLauncherCommand,
    "is-enabled-launcher",
    "Check if the launcher is enabled"
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
    if (query(launcher, {"is-enabled"})) {
      if (!quiet) {
        qStdOut() << "enabled\n";
      }
      return EXIT_SUCCESS;
    }
    if (!quiet) {
      qStdOut() << "disabled\n";
    }
    return EXIT_FAILURE;
  }

private:
  QCommandLineOption quietOption =
    QCommandLineOption("quiet", "Only return exit code");
};
