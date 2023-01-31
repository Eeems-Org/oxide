#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <liboxide/applications.h>

#include <common.h>
#include <cat.h>
#include <help.h>
#include <version.h>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("gio", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("gio");
    app.setApplicationVersion(APP_VERSION);

    Q_UNUSED(new HelpCommand());
    Q_UNUSED(new VersionCommand());
    Q_UNUSED(new CatCommand());

    QCommandLineParser parser;
    parser.setApplicationDescription("GIO command line tool");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addOption(ICommand::versionOption());
    parser.addPositionalArgument("command", ICommand::commandsHelp());
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
    parser.parse(app.arguments());
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    return ICommand::exec(parser);
}