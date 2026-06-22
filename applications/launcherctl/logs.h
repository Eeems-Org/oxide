#pragma once

#include "common.h"

#include <QCommandLineOption>

class LogsCommand : ICommand {
  O_COMMAND(LogsCommand, "logs", "View current launcher logs", true)
  int arguments(QCommandLineParser& parser) override {
    parser.addOption(followOption);
    return EXIT_SUCCESS;
  }
  int command(QCommandLineParser& parser, const QStringList& args) override {
    if (!args.isEmpty()) {
      parser.showHelp(EXIT_FAILURE);
    }
    checkActiveLaunchers();
    auto launcher = launchersActive().first();
    QStringList scriptArgs = {"logs"};
    if (parser.isSet("follow")) {
      scriptArgs.append("--follow");
    }
    auto path = QString(SHARE_DIR) + "/" + launcher;
    QProcess proc;
    proc.setProgram(path);
    proc.setArguments(scriptArgs);
    proc.setInputChannelMode(QProcess::ForwardedInputChannel);
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start();
    proc.waitForFinished(-1);
    return proc.exitCode();
  }

private:
  QCommandLineOption followOption =
    QCommandLineOption({"f", "follow"}, "Follow log output");
};
