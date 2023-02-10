#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>

#include <string>
#include <liboxide.h>
#include <liboxide/applications.h>

#include "edit.h"

using namespace Oxide::Sentry;
using namespace Oxide::JSON;
using namespace Oxide::Applications;

QTextStream& qStdOut(){
    static QTextStream ts( stdout );
    return ts;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("desktop-file-edit", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("desktop-file-edit");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Edit application registration files");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    addEditOptions(parser);
    parser.addPositionalArgument("FILE", "Application registration to edit");
    parser.process(app);
    auto args = parser.positionalArguments();
    if(args.empty() || args.length() > 1){
        parser.showHelp(EXIT_FAILURE);
    }
    if(!validateSetKeyValueOptions(parser)){
        return EXIT_FAILURE;
    }
    auto path = args.first();
    QFile file(path);
    if(!file.exists()){
        qDebug() << "Error on file" << path << ": No such file or directory";
        return EXIT_FAILURE;
    }
    auto reg = getRegistration(&file);
    file.close();
    if(!file.open(QFile::WriteOnly | QFile::Truncate)){
        qDebug() << "Error on file" << path << ": Cannot write to file";
        return EXIT_FAILURE;
    }
    if(reg.isEmpty()){
        qDebug() << "Error on file" << path << ": is not valid";
        return EXIT_FAILURE;
    }

    QFileInfo info(file);
    auto name = info.baseName();
    applyChanges(parser, reg, name);
    auto json = toJson(reg, QJsonDocument::Indented);
    file.write(json.toUtf8());
    file.close();
    bool success = true;
    for(auto error : validateRegistration(name, reg)){
        qStdOut() << path << ": " << error << Qt::endl;
        auto level = error.level;
        if(level == ErrorLevel::Error || level == ErrorLevel::Critical){
            success = false;
        }
    }
    if(info.suffix() != "oxide"){
        qDebug() << path.toStdString().c_str() << ": error: filename does not have a .oxide extension";
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
