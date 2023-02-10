#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>

#include <string>
#include <liboxide.h>
#include <liboxide/applications.h>

#include "../desktop-file-edit/edit.h"

using namespace Oxide::Sentry;
using namespace Oxide::JSON;
using namespace Oxide::Applications;

QTextStream& qStdOut(){
    static QTextStream ts( stdout );
    return ts;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("desktop-file-install", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("desktop-file-install");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Install application registration files");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dirOption(
        "dir",
        "Install desktop files to the DIR directory.",
        "DIR",
        OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY
    );
    parser.addOption(dirOption);
    QCommandLineOption modeOption(
        {"m", "mode"},
        "NOT IMPLEMENTED",
        "MODE"
    );
    parser.addOption(modeOption);
    QCommandLineOption vendorOption(
        "vendor",
        "NOT IMPLEMENTED",
        "VENDOR"
    );
    parser.addOption(vendorOption);
    QCommandLineOption deleteOriginalOption(
        "delete-original",
        "Delete the source registration files, leaving only the target files. Effectively \"renames\" the registration files."
    );
    parser.addOption(deleteOriginalOption);
    QCommandLineOption rebuildMimeInfoCacheOption(
        "rebuild-mime-info-cache",
        "NOT IMPLEMENTED"
    );
    parser.addOption(rebuildMimeInfoCacheOption);
    addEditOptions(parser);
    parser.addPositionalArgument("FILE", "Application registration(s) to install", "FILE...");
    parser.process(app);
    auto args = parser.positionalArguments();
    if(args.empty()){
        parser.showHelp(EXIT_FAILURE);
    }
    if(!validateSetKeyValueOptions(parser)){
        return EXIT_FAILURE;
    }
    bool success = true;
    for(auto path : args){
        QFile file(path);
        if(!file.exists()){
            qDebug() << "Error on file" << path << ": No such file or directory";
            success = false;
            continue;
        }
        auto reg = getRegistration(&file);
        file.close();
        QFileInfo info(file);
        QFile toFile(QDir::cleanPath(parser.value(dirOption) + QDir::separator() + info.baseName() + ".oxide"));
        if(!toFile.open(QFile::WriteOnly | QFile::Truncate)){
            qDebug() << "Error on file" << path << ": Cannot write to file";
            success = false;
            continue;
        }
        if(info.suffix() != "oxide"){
            qDebug() << path.toStdString().c_str() << ": error: filename does not have a .oxide extension";
            success = false;
            continue;
        }
        if(reg.isEmpty()){
            qDebug() << "Error on file" << path << ": is not valid";
            success = false;
            continue;
        }

        auto name = info.baseName();
        applyChanges(parser, reg, name);
        bool hadError = false;
        for(auto error : validateRegistration(name, reg)){
            qStdOut() << path << ": " << error << Qt::endl;
            auto level = error.level;
            if(level == ErrorLevel::Error || level == ErrorLevel::Critical){
                hadError = true;
            }
        }
        if(hadError){
            success = false;
            continue;
        }
        auto json = toJson(reg, QJsonDocument::Indented);
        toFile.write(json.toUtf8());
        toFile.close();
        if(parser.isSet(deleteOriginalOption)){
            file.remove();
        }
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
