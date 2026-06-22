#include "active-launcher.h"
#include "common.h"
#include "current-launcher.h"
#include "is-active-launcher.h"
#include "is-current-launcher.h"
#include "is-enabled-launcher.h"
#include "list-apps.h"
#include "list-launchers.h"
#include "list-paused-apps.h"
#include "list-running-apps.h"
#include "logs.h"
#include "pause-app.h"
#include "resume-app.h"
#include "start-app.h"
#include "start-launcher.h"
#include "status.h"
#include "stop-app.h"
#include "stop-launcher.h"
#include "switch-launcher.h"

#include <liboxide.h>

#include <QCommandLineParser>
#include <QDir>

using namespace Oxide::Sentry;

class VersionCommand : ICommand {
public:
  VersionCommand()
    : ICommand(true) {}
  int command(QCommandLineParser& parser, const QStringList& args) override {
    Q_UNUSED(args);
    parser.showVersion();
    return EXIT_SUCCESS;
  }
};

int
main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  sentry_init("launcherctl", argv);
  app.setOrganizationName("Eeems");
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("launcherctl");
  app.setApplicationVersion(APP_VERSION);
  QDir().mkpath(SHARE_DIR);

  STATIC_INSTANCE(ActiveLauncherCommand);
  STATIC_INSTANCE(StatusCommand);
  STATIC_INSTANCE(CurrentLauncherCommand);
  STATIC_INSTANCE(ListLaunchersCommand);
  STATIC_INSTANCE(SwitchLauncherCommand);
  STATIC_INSTANCE(StartLauncherCommand);
  STATIC_INSTANCE(StopLauncherCommand);
  STATIC_INSTANCE(ListAppsCommand);
  STATIC_INSTANCE(ListRunningAppsCommand);
  STATIC_INSTANCE(ListPausedAppsCommand);
  STATIC_INSTANCE(StartAppCommand);
  STATIC_INSTANCE(StopAppCommand);
  STATIC_INSTANCE(PauseAppCommand);
  STATIC_INSTANCE(ResumeAppCommand);
  STATIC_INSTANCE(IsCurrentLauncherCommand);
  STATIC_INSTANCE(IsEnabledLauncherCommand);
  STATIC_INSTANCE(IsActiveLauncherCommand);
  STATIC_INSTANCE(LogsCommand);
  STATIC_INSTANCE(VersionCommand);

  QCommandLineParser parser;
  parser.setApplicationDescription("Launcher control tool");
  parser.addHelpOption();
  parser.addPositionalArgument(
    "Commands:", ICommand::commandsHelp(), "COMMAND"
  );
  parser.setOptionsAfterPositionalArgumentsMode(
    QCommandLineParser::ParseAsPositionalArguments
  );
  parser.parse(app.arguments());
  parser.setOptionsAfterPositionalArgumentsMode(
    QCommandLineParser::ParseAsOptions
  );
  return ICommand::exec(parser);
}
