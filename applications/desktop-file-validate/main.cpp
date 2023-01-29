#include <QCommandLineParser>
#include <QJsonValue>
#include <QJsonDocument>
#include <QSet>
#include <QMutableListIterator>

#include <liboxide.h>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide::JSON;

int qExit(int ret){
    QTimer::singleShot(0, [ret](){
        qApp->exit(ret);
    });
    return qApp->exec();
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
    parser.process(app);
    if(!parser.positionalArguments().empty()){
        parser.showHelp(EXIT_FAILURE);
    }
    auto bus = QDBusConnection::systemBus();
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    QDBusObjectPath path = api.requestAPI("apps");
    if(path.path() == "/"){
        qDebug() << "Unable to get apps API";
        return qExit(EXIT_FAILURE);
    }
    Apps apps(OXIDE_SERVICE, path.path(), bus);

    return qExit(EXIT_SUCCESS);
}
