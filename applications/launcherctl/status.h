#pragma once

#include "common.h"

class StatusCommand : ICommand {
  O_COMMAND(StatusCommand, "status", "Current launcher status", true)
  int arguments(QCommandLineParser& parser) override {
    Q_UNUSED(parser);
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (!args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto launcher = currentLauncher();
    if (launcher.isEmpty()) {
      qStdErr() << "Error: No launcher currently set\n";
      return EXIT_FAILURE;
    }
    qStdOut() << "Launcher: \033[1m" << launcher << "\e[0m\n";
    auto active = launchersActive();
    qStdOut() << "Status: \033[1m";
    if (active.isEmpty()) {
      qStdOut() << "\e[31minactive\e[0m\n";
    } else {
      qStdOut() << "\e[32mactive\e[0m <" << active.join(", ") << ">\n";
    }
    QString output;
    queryActive({"running"}, &output);
    auto running = output.split('\n', Qt::SkipEmptyParts).size();
    output.clear();
    queryActive({"paused"}, &output);
    auto paused = output.split('\n', Qt::SkipEmptyParts).size();
    output.clear();
    queryCurrent({"apps"}, &output);
    auto apps = output.split('\n', Qt::SkipEmptyParts).size();
    output.clear();
    qStdOut() << "Apps: " << running << " running, " << paused
              << " paused, and " << apps << " installed\n";
    return EXIT_SUCCESS;
  }
};
