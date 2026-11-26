#include <common.h>
#include <default.h>
#include <query.h>
#include <sys/stat.h>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

int
main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  sentry_init("xdg-mime", argv);
  app.setOrganizationDomain(OXIDE_SERVICE);
  app.setApplicationName("xdg-mime");
  app.setApplicationVersion(APP_VERSION);

  STATIC_INSTANCE(QueryCommand);
  STATIC_INSTANCE(DefaultCommand);
  O_COMMAND_STUB(install);
  O_COMMAND_STUB(uninstall);

  QCommandLineParser parser;
  parser.setApplicationDescription(
    "command line tool for querying information about\n"
    "file type handling and adding descriptions for new file types"
  );
  parser.applicationDescription();
  parser.addHelpOption();
  parser.addOption(ICommand::versionOption());
  parser.addPositionalArgument(
    "Commands:\n", ICommand::commandsHelp(), "COMMAND"
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
