#pragma once

#include "common.h"
#include <cstdlib>

class StartLauncherCommand : ICommand {
  O_COMMAND(
    StartLauncherCommand,
    "start-launcher",
    "Start the current launcher",
    true
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addOption(noStopOption);
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (!args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    if (!parser.isSet("no-stop")) {
      auto selectedLauncher = currentLauncher();
      for (const auto& launcher : launchersActive()) {
        if (launcher != selectedLauncher) {
          query(launcher, {"stop"});
        }
      }
    }
    if (query("current", {"is-active"})) {
      return EXIT_SUCCESS;
    }
    return query("current", {"start"}) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

private:
  QCommandLineOption noStopOption =
    QCommandLineOption("no-stop", "Don't stop other launchers");
};
