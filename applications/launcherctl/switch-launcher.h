#pragma once

#include "common.h"

#include <QCommandLineOption>

class SwitchLauncherCommand : ICommand {
  O_COMMAND(
    SwitchLauncherCommand,
    "switch-launcher",
    "Switch which launcher is active"
  )
  int arguments(QCommandLineParser& parser) override {
    parser.addOption(startOption);
    parser.addPositionalArgument("launcher", "The launcher to switch to");
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    auto launcher = args.first();
    auto path = QString(SHARE_DIR) + "/" + launcher;
    if (!QFile::exists(path)) {
      qStdOut() << launcher << " is not installed\n";
      return EXIT_FAILURE;
    }
    for (const auto& l : launchers()) {
      if (l != launcher) {
        query(l, {"disable"});
      }
    }
    if (!query(launcher, {"is-enabled"})) {
      if (!query(launcher, {"enable"})) {
        return EXIT_FAILURE;
      }
    }
    auto currentPath = QString(SHARE_DIR) + "/current";
    QFile currentLink(currentPath);
    if (
      currentLink.exists() &&
      QFileInfo(currentPath).canonicalFilePath() !=
        QFileInfo(path).canonicalFilePath() &&
      !currentLink.remove()
    ) {
      qStdErr() << "Error: Failed to remove current link: "
                << currentLink.errorString() << "\n";
      return EXIT_FAILURE;
    }
    if (!currentLink.exists()) {
      QFile launcherFile(QString(SHARE_DIR) + "/" + launcher);
      if (!launcherFile.link(currentPath)) {
        qStdErr() << "Error: Failed to create current link: "
                  << launcherFile.errorString() << "\n";
        return EXIT_FAILURE;
      }
    }
    if (parser.isSet("start")) {
      return ICommand::exec({"start-launcher"});
    }
    return EXIT_SUCCESS;
  }

private:
  QCommandLineOption startOption =
    QCommandLineOption("start", "Start the launcher after switching");
};
