#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <liboxide/applications.h>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;

#define APPS_DIR OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY

QCommandLineOption versionOption(
    {"v", "version"},
    "Displays version information."
);

QStringList positionArguments(QCommandLineParser& parser){
    parser.process(*qApp);
    if(parser.isSet(versionOption)){
        parser.showHelp(EXIT_FAILURE);
    }
    auto args = parser.positionalArguments();
    args.removeFirst();
    if(args.isEmpty() || args.count() > 1){
        parser.showHelp(EXIT_FAILURE);
    }
    return args;
}

int install(QCommandLineParser& parser){
    parser.addPositionalArgument("install", "Install one or more application registration.", "install [options]");
    QCommandLineOption novendorOption(
        "novendor",
        "NOT IMPLEMENETED"
    );
    parser.addOption(novendorOption);
    parser.addPositionalArgument("file", "Application registration to install.", "FILE");

    QString path = positionArguments(parser).first();
    if(!QFile::exists(path)){
        qDebug() << "error:" << path.toStdString().c_str() << "does not exist";
        return EXIT_FAILURE;
    }
    if(!QFile::exists(APPS_DIR) && mkdir(APPS_DIR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)){
        qDebug() << "error: failed to created " APPS_DIR;
        return EXIT_FAILURE;
    }
    QFileInfo info(path);
    if(info.absoluteDir().path() == APPS_DIR){
        qDebug() << "error: " << path.toStdString().c_str() << "is already installed";
        return EXIT_FAILURE;
    }
    auto errors = validateRegistration(path);
    if(std::any_of(errors.constBegin(), errors.constEnd(), [](const ValidationError& e){
        return e.level == ErrorLevel::Error || e.level == ErrorLevel::Critical;
    })){
        qDebug() << "error: " << path.toStdString().c_str() << "is not valid";
        return EXIT_FAILURE;
    }
    auto toPath = APPS_DIR "/" + info.fileName();
    if(QFile::exists(toPath) && !QFile::remove(toPath)){
        qDebug() << "error: " << toPath.toStdString().c_str() << " already exists and can't be removed";
        return EXIT_FAILURE;
    }
    if(!QFile::copy(path, toPath)){
        qDebug() << "error: Failed to created" << toPath.toStdString().c_str();
        return EXIT_FAILURE;
    }
    qDebug() << "success: Installed" << path.toStdString().c_str();
    return QProcess::execute("update-desktop-database", QStringList("--quiet"));
}
int uninstall(QCommandLineParser& parser){
    parser.addPositionalArgument("uninstall", "Uninstall one or more application registration.", "uninstall");
    parser.addPositionalArgument("file", "Application registration to uninstall.", "FILE");

    QString path = positionArguments(parser).first();
    if(!QFile::exists(path)){
        qDebug() << "error:" << path.toStdString().c_str() << "does not exist";
        return EXIT_FAILURE;
    }
    auto toPath = APPS_DIR "/" + QFileInfo(path).fileName();
    if(!QFile::exists(toPath)){
        qDebug() << "success:" << toPath.toStdString().c_str() << "doesn't exist";
    }else if(!QFile::remove(toPath)){
        qDebug() << "error: Could not remove" << toPath.toStdString().c_str();
        return EXIT_FAILURE;
    }else{
        qDebug() << "success: Uninstalled" << toPath.toStdString().c_str();
    }
    return QProcess::execute("update-desktop-database", QStringList("--quiet"));
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("xdg-desktop-icon", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("xdg-desktop-icon");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("A program to (un)install application registrations");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addOption(versionOption);
    parser.addPositionalArgument("command",
        "install      Install one or more application registration.\n"
        "uninstall    Uninstall one or more application registration.\n"
    );

    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
    parser.parse(app.arguments());
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(EXIT_FAILURE);
    }
    if(parser.isSet(versionOption)){
        parser.showVersion();
    }

    auto command = args.first();
    if(command == "install"){
        parser.clearPositionalArguments();
        return install(parser);
    }
    if(command == "uninstall"){
        parser.clearPositionalArguments();
        return uninstall(parser);
    }
    parser.showHelp(EXIT_FAILURE);
}
