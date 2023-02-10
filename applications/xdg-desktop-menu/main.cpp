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

QStringList positionArguments(QCommandLineParser& parser, bool allowEmpty = false){
    parser.process(*qApp);
    if(parser.isSet(versionOption)){
        parser.showHelp(EXIT_FAILURE);
    }
    auto args = parser.positionalArguments();
    args.removeFirst();
    if(!allowEmpty && args.isEmpty()){
        parser.showHelp(EXIT_FAILURE);
    }
    return args;
}

int install(QCommandLineParser& parser){
    parser.addPositionalArgument("install", "Install one or more application registration.", "install [options]");
    QCommandLineOption noupdateOption(
        "noupdate",
        "Do not update the application cache."
    );
    parser.addOption(noupdateOption);
    QCommandLineOption novendorOption(
        "novendor",
        "NOT IMPLEMENETED"
    );
    parser.addOption(novendorOption);
    QCommandLineOption modeOption(
        "mode",
        "NOT IMPLEMENETED",
        "mode"
    );
    parser.addOption(modeOption);
    parser.addPositionalArgument("files", "Application registration(s) to install.", "directory-file(s) desktop-file(s)");

    auto args = positionArguments(parser);
    if(!QFile::exists(APPS_DIR) && mkdir(APPS_DIR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)){
        qDebug() << "error: failed to created " APPS_DIR;
        return EXIT_FAILURE;
    }
    bool failure = false;
    bool updated = false;
    for(QString path : args){
        if(!QFile::exists(path)){
            qDebug() << "error:" << path.toStdString().c_str() << "does not exist";
            failure = true;
            continue;
        }
        QFileInfo info(path);
        if(info.absoluteDir().path() == APPS_DIR){
            qDebug() << "error: " << path.toStdString().c_str() << "is already installed";
            failure = true;
            continue;
        }
        auto errors = validateRegistration(path);
        if(std::any_of(errors.constBegin(), errors.constEnd(), [](const ValidationError& e){
            return e.level == ErrorLevel::Error || e.level == ErrorLevel::Critical;
        })){
            qDebug() << "error: " << path.toStdString().c_str() << "is not valid";
            failure = true;
            continue;
        }
        auto toPath = APPS_DIR "/" + info.fileName();
        if(QFile::exists(toPath) && !QFile::remove(toPath)){
            failure = true;
            updated = true;
            qDebug() << "error: " << toPath.toStdString().c_str() << " already exists and can't be removed";
            continue;
        }
        if(!QFile::copy(path, toPath)){
            qDebug() << "error: Failed to created" << toPath.toStdString().c_str();
            failure = true;
            continue;
        }
        qDebug() << "success: Installed" << path.toStdString().c_str();
        updated = true;
    }

    if(!updated || parser.isSet(noupdateOption)){
        return failure ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    int res = QProcess::execute("update-desktop-database", QStringList("--quiet"));
    return failure ? EXIT_FAILURE : res;
}
int uninstall(QCommandLineParser& parser){
    parser.addPositionalArgument("uninstall", "Uninstall one or more application registration.", "uninstall [options]");
    QCommandLineOption noupdateOption(
        "noupdate",
        "Do not update the application cache."
    );
    parser.addOption(noupdateOption);
    QCommandLineOption modeOption(
        "mode",
        "NOT IMPLEMENETED",
        "mode"
    );
    parser.addOption(modeOption);
    parser.addPositionalArgument("files", "Application registration(s) to install.", "directory-file(s) desktop-file(s)");

    auto args = positionArguments(parser);
    bool failure = false;
    bool updated = false;
    for(QString path : args){
        if(!QFile::exists(path)){
            qDebug() << "error:" << path.toStdString().c_str() << "does not exist";
            failure = true;
            continue;
        }
        auto toPath = APPS_DIR "/" + QFileInfo(path).fileName();
        if(!QFile::exists(toPath)){
            qDebug() << "success:" << toPath.toStdString().c_str() << "doesn't exist";
            continue;
        }
        if(!QFile::remove(toPath)){
            failure = true;
            qDebug() << "error: Could not remove" << toPath.toStdString().c_str();
            continue;
        }
        qDebug() << "success: Uninstalled" << toPath.toStdString().c_str();
        updated = true;
    }
    if(!updated || parser.isSet(noupdateOption)){
        return failure ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    int res = QProcess::execute("update-desktop-database", QStringList("--quiet"));
    return failure ? EXIT_FAILURE : res;
}

int forceupdate(QCommandLineParser& parser){
    parser.addPositionalArgument("forceupdate", "Force an update of the application registration cache.", "forceupdate [options]");
    QCommandLineOption modeOption(
        "mode",
        "NOT IMPLEMENETED",
        "mode"
    );
    parser.addOption(modeOption);

    if(!positionArguments(parser, true).isEmpty()){
        parser.showHelp();
    }
    return QProcess::execute("update-desktop-database", QStringList());
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("xdg-desktop-menu", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("xdg-desktop-menu");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("A program to (un)install application registrations");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addOption(versionOption);
    parser.addPositionalArgument("command",
        "install      Install one or more application registration.\n"
        "uninstall    Uninstall one or more application registration.\n"
        "forceupdate  Force an update of the application registration \n"
        "              cache.\n"
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
    if(command == "forceupdate"){
        parser.clearPositionalArguments();
        return forceupdate(parser);
    }
    parser.showHelp(EXIT_FAILURE);
}
