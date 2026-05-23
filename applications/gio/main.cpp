#include <cat.h>
#include <common.h>
#include <copy.h>
#include <help.h>
#include <launch.h>
#include <liboxide.h>
#include <liboxide/applications.h>
#include <mkdir.h>
#include <open.h>
#include <remove.h>
#include <rename.h>
#include <sys/stat.h>
#include <version.h>

#include <QCommandLineParser>
#include <string>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;

int
main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    sentry_init("gio", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("gio");
    app.setApplicationVersion(APP_VERSION);

    STATIC_INSTANCE(HelpCommand);
    STATIC_INSTANCE(VersionCommand);
    STATIC_INSTANCE(CatCommand);
    STATIC_INSTANCE(CopyCommand);
    O_COMMAND_STUB(info);
    STATIC_INSTANCE(LaunchCommand);
    O_COMMAND_STUB(list);
    O_COMMAND_STUB(mime);
    STATIC_INSTANCE(MkdirCommand);
    O_COMMAND_STUB(monitor);
    O_COMMAND_STUB(mount);
    O_COMMAND_STUB(move);
    STATIC_INSTANCE(OpenCommand);
    STATIC_INSTANCE(RenameCommand);
    STATIC_INSTANCE(RemoveCommand);
    O_COMMAND_STUB(save);
    O_COMMAND_STUB(set);
    O_COMMAND_STUB(trash);
    O_COMMAND_STUB(tree);

    QCommandLineParser parser;
    parser.setApplicationDescription("GIO command line tool");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addOption(ICommand::versionOption());
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
