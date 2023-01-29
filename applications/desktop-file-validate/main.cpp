#include <QCommandLineParser>

#include <string>
#include <liboxide.h>
#include <liboxide/applications.h>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;

int qExit(int ret){
    QTimer::singleShot(0, [ret](){
        qApp->exit(ret);
    });
    return qApp->exec();
}
QTextStream& qStdOut(){
    static QTextStream ts( stdout );
    return ts;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("desktop-file-validate", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("desktop-file-validate");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("A program to send desktop notifications");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noHintsOption(
        "no-hints",
        "Do not output hints about things that might be improved in the application registration."
    );
    parser.addOption(noHintsOption);
    QCommandLineOption noWarnDeprecatedOption(
        "no-warn-deprecated",
        "Do not warn about usage of deprecated items that were defined in the previous version of the specification."
    );
    parser.addOption(noWarnDeprecatedOption);
    QCommandLineOption warnKDEOption("warn-kde", "NOT IMPLEMENTED");
    parser.addOption(warnKDEOption);
    parser.addPositionalArgument("file", "Application registration(s) to validate", "FILE...");
    parser.process(app);
    auto args = parser.positionalArguments();
    if(args.empty()){
        parser.showHelp(EXIT_FAILURE);
    }
    for(QString path : args){
        for(auto error : validateRegistration(path)){
            qStdOut() << path << ": " << error << Qt::endl;
        }
    }
    return qExit(EXIT_SUCCESS);
}
