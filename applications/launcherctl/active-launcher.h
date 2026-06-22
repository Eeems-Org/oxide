#pragma once

#include "common.h"

class ActiveLauncherCommand : ICommand {
  O_COMMAND(
    ActiveLauncherCommand,
    "active-launcher",
    "Get the name of the active launcher",
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
    auto active = launchersActive();
    if (active.isEmpty()) {
      qStdErr() << "Error: No launcher is currently active\n";
      return EXIT_FAILURE;
    }
    if (active.size() > 1) {
      qStdErr() << "Error: More than one launcher is currently active\n";
      return EXIT_FAILURE;
    }
    qStdOut() << active.first() << "\n";
    return EXIT_SUCCESS;
  }
};
