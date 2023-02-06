#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <liboxide/applications.h>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;

#define ICON_DIR OXIDE_ICONS_DIRECTORY

QCommandLineOption versionOption(
    {"v", "version"},
    "Displays version information."
);

QCommandLineOption themeOption(
    "theme",
    "Installs or removes the icon file as part of theme. If no theme is specified the icons will be installed as part of the default hicolor theme. Applications may install icons under multiple themes but should at least install icons for the default hicolor theme.",
    "theme",
    "hicolor"
);

QCommandLineOption modeOption(
    "mode",
    "NOT IMPLEMENETED",
    "mode"
);
QCommandLineOption noupdateOption(
    "noupdate",
    "Do not update the application cache."
);
QCommandLineOption sizeOption(
    "size",
    "Specifies the size of the icon. All icons must be square. Common sizes for icons in the apps context are: 16, 22, 32, 48, 64 and 128. Common sizes for icons in the mimetypes context are: 16, 22, 32, 48, 64 and 128",
    "size"
);
QCommandLineOption contextOption(
    "context",
    "Specifies the context for the icon. Icons to be used in the application menu and as desktop icon should use apps as context which is the default context. Icons to be used as file icons should use mimetypes as context. Other common contexts are actions, devices, emblems, filesystems and stock.",
    "context",
    "apps"
);
QCommandLineOption novendorOption(
    "novendor",
    "NOT IMPLEMENETED"
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
QString iconDir(QCommandLineParser& parser){ return Oxide::Applications::iconDirPath(
    parser.value(sizeOption).toUInt(),
    parser.value(themeOption),
    parser.value(contextOption)
); }
QString iconFile(QCommandLineParser& parser, const QString& name){ return Oxide::Applications::iconPath(
    name,
    parser.value(sizeOption).toUInt(),
    parser.value(themeOption),
    parser.value(contextOption)
); }

int install(QCommandLineParser& parser){
    parser.addPositionalArgument("", "", "install [options]");
    parser.addOption(noupdateOption);
    parser.addOption(novendorOption);
    parser.addOption(themeOption);
    parser.addOption(modeOption);
    parser.addOption(contextOption);
    parser.addOption(sizeOption);
    parser.addPositionalArgument("icon-file", "Icon file to install.");
    parser.addPositionalArgument("icon-name", "Name to use when installing.", "[icon-name]");

    auto args = positionArguments(parser);
    if(args.length() > 2 || !parser.isSet(sizeOption)){
        parser.showHelp(EXIT_FAILURE);
    }

    auto fromPath = args.first();
    if(!QFile::exists(fromPath)){
        qDebug() << "error: file does not exist" << fromPath;
        return EXIT_FAILURE;
    }

    auto dirPath = iconDir(parser);
    QDir dir(dirPath);
    if(!dir.exists() && !dir.mkpath(".")){
        qDebug() << "error: failed to created directory" << dirPath;
        return EXIT_FAILURE;
    }

    QString name = args.length() == 2 ? args.last() : QFileInfo(fromPath).baseName();
    auto toPath = iconFile(parser, name);
    if(!QFile::copy(fromPath, toPath)){
        qDebug() << "error: failed to copy file to" << toPath;
        return EXIT_FAILURE;
    }

    if(parser.isSet(noupdateOption)){
        return EXIT_SUCCESS;
    }
    return QProcess::execute("update-desktop-database", QStringList("--quiet"));
}
int uninstall(QCommandLineParser& parser){
    parser.addPositionalArgument("", "", "uninstall [options]");
    parser.addOption(noupdateOption);
    parser.addOption(themeOption);
    parser.addOption(modeOption);
    parser.addOption(contextOption);
    parser.addOption(sizeOption);
    parser.addPositionalArgument("icon-name", "Name to use when installing.", "[icon-name]");

    auto args = positionArguments(parser);
    if(args.length() > 1 || !parser.isSet(sizeOption)){
        parser.showHelp(EXIT_FAILURE);
    }

    auto path = iconFile(parser, args.last());
    if(QFile::exists(path) && !QFile::remove(path)){
        qDebug() << "error: failed to delete" << path;
        return EXIT_FAILURE;
    }

    if(parser.isSet(noupdateOption)){
        return EXIT_SUCCESS;
    }
    return QProcess::execute("update-desktop-database", QStringList("--quiet"));
}

int forceupdate(QCommandLineParser& parser){
    parser.addPositionalArgument("", "", "forceupdate [options]");
    parser.addOption(themeOption);
    parser.addOption(modeOption);

    if(!positionArguments(parser, true).isEmpty()){
        parser.showHelp();
    }
    return QProcess::execute("update-desktop-database", QStringList());
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("xdg-icon-resource", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("xdg-icon-resource");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Command line tool for (un)installing icon resources.");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addOption(versionOption);
    parser.addPositionalArgument("command",
        "install      Installs the icon file indicated by icon-file to\n"
        "              the desktop icon system under the name icon-name.\n"
        "              Icon names do not have an extension. If icon-name\n"
        "              is not provided the name is derived from\n"
        "              icon-file.\n"
        "uninstall    Removes the icon indicated by icon-name from the\n"
        "              desktop icon system. Note that icon names do not\n"
        "              have an extension.\n"
        "forceupdate  Force an update of the desktop icon system. This\n"
        "              is only useful if the last call to\n"
        "              xdg-icon-resource included the --noupdate option.\n"
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
