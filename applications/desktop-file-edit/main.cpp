#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>

#include <string>
#include <liboxide.h>
#include <liboxide/applications.h>

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

    QCommandLineOption setKeyOption(
        "set-key",
        "Set the KEY key to the value passed to the next --set-value option. A matching --set-value option is mandatory.",
        "KEY"
    );
    parser.addOption(setKeyOption);
    QCommandLineOption setValueOption(
        "set-value",
        "Set the key specified with the previous --set-key option to VALUE. A matching --set-key option is mandatory.",
        "VALUE"
    );
    parser.addOption(setValueOption);
    QCommandLineOption setNameOption(
        "set-name",
        "NOT IMPLEMENTED",
        "NAME"
    );
    parser.addOption(setNameOption);
    QCommandLineOption copyNameToGenericNameOption(
        "copy-name-to-generic-name",
        "Copy the value of the Name key to the GenericName key. Note that a desktop file requires a Name key to be valid, so this option will always have an effect."
    );
    parser.addOption(copyNameToGenericNameOption);
    QCommandLineOption setGenericNameOption(
        "set-generic-name",
        "Set the generic name (key DisplayName) to GENERIC-NAME. If a generic name was already set, it will be overridden. Localizations of the old generic name will be removed.",
        "GENERIC-NAME"
    );
    parser.addOption(setGenericNameOption);
    QCommandLineOption copyGenericNameToNameOption(
        "copy-generic-name-to-name",
        "NOT IMPLEMENTED"
    );
    parser.addOption(copyGenericNameToNameOption);
    QCommandLineOption setCommentOption(
        "set-comment",
        "NOT IMPLEMENTED",
        "COMMENT"
    );
    parser.addOption(setCommentOption);
    QCommandLineOption setIconOption(
        "set-icon",
        "Set the icon (key Icon) to ICON. If an icon was already set, it will be overridden. Localizations of the old icon will be removed.",
        "ICON"
    );
    parser.addOption(setIconOption);
    QCommandLineOption addCategoryOption(
        "add-category",
        "NOT IMPLEMENTED",
        "CATEGORY"
    );
    parser.addOption(addCategoryOption);
    QCommandLineOption removeCategoryOption(
        "remove-category",
        "NOT IMPLEMENTED",
        "CATEGORY"
    );
    parser.addOption(removeCategoryOption);
    QCommandLineOption addMimeTypeOption(
        "add-mime-type",
        "NOT IMPLEMENTED",
        "MIME-TYPE"
    );
    parser.addOption(addMimeTypeOption);
    QCommandLineOption removeMimeTypeOption(
        "remove-mime-type",
        "NOT IMPLEMENTED",
        "MIME-TYPE"
    );
    parser.addOption(removeMimeTypeOption);
    QCommandLineOption addOnlyShowInOption(
        "add-only-show-in",
        "NOT IMPLEMENTED",
        "ENVIRONMENT"
    );
    parser.addOption(addOnlyShowInOption);
    QCommandLineOption removeOnlyShowInOption(
        "remove-only-show-in",
        "NOT IMPLEMENTED",
        "ENVIRONMENT"
    );
    parser.addOption(removeOnlyShowInOption);
    QCommandLineOption addNotShowInOption(
        "add-not-show-in",
        "NOT IMPLEMENTED",
        "ENVIRONMENT"
    );
    parser.addOption(addNotShowInOption);
    QCommandLineOption removeNotShowInOption(
        "remove-not-show-in",
        "NOT IMPLEMENTED",
        "ENVIRONMENT"
    );
    parser.addOption(removeNotShowInOption);
    QCommandLineOption removeKeyOption(
        "remove-key",
        "Remove the KEY key from the desktop files, if present.",
        "KEY"
    );
    parser.addOption(removeKeyOption);

    parser.addPositionalArgument("FILE", "Application registration to edit");
    parser.process(app);
    auto args = parser.positionalArguments();
    if(args.empty() || args.length() > 1){
        parser.showHelp(EXIT_FAILURE);
    }
    auto options = parser.optionNames();
    if(parser.isSet(setKeyOption) || parser.isSet(setValueOption)){
        for(int i=0; i< options.length(); i++){
            auto option = options[i];
            if(setValueOption.names().contains(option)){
                qDebug() << "Option \"--set-value\" used without a prior \"--set-key\" option";
                qDebug() << "Run 'desktop-file-edit --help' to see a full list of available command line options.";
                return EXIT_FAILURE;
            }
            if(setKeyOption.names().contains(option)){
                option = options[++i];
                if(!setValueOption.names().contains(option)){
                    qDebug() << "Option \"--set-key\" used without a following \"--set-value\" option";
                    qDebug() << "Run 'desktop-file-edit --help' to see a full list of available command line options.";
                    return EXIT_FAILURE;
                }
            }
        }
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
    int removeIndex = 0;
    int keyIndex = 0;
    int genericNameIndex = 0;
    int iconIndex = 0;
    QString key;
    for(int i=0; i< options.length(); i++){
        auto option = options[i];
        if(copyNameToGenericNameOption.names().contains(option)){
            reg["displayName"] = name;
            continue;
        }
        if(removeKeyOption.names().contains(option)){
            reg.remove(parser.values(removeKeyOption)[removeIndex]);
            removeIndex++;
            continue;
        }
        if(setGenericNameOption.names().contains(option)){
            reg["displayname"] = parser.values(setGenericNameOption)[genericNameIndex];
            genericNameIndex++;
            continue;
        }
        if(setIconOption.names().contains(option)){
            reg["icon"] = parser.values(setIconOption)[iconIndex];
            iconIndex++;
            continue;
        }
        if(setKeyOption.names().contains(option)){
            key = parser.values(setKeyOption)[keyIndex];
            reg[key] = parser.values(setValueOption)[keyIndex];
            i++;
            keyIndex++;
        }
    }

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
