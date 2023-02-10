#include "edit.h"

QCommandLineOption setKeyOption(
    "set-key",
    "Set the KEY key to the value passed to the next --set-value option. A matching --set-value option is mandatory.",
    "KEY"
);
QCommandLineOption setValueOption(
    "set-value",
    "Set the key specified with the previous --set-key option to VALUE. A matching --set-key option is mandatory.",
    "VALUE"
);
QCommandLineOption setNameOption(
    "set-name",
    "NOT IMPLEMENTED",
    "NAME"
);
QCommandLineOption copyNameToGenericNameOption(
    "copy-name-to-generic-name",
    "Copy the value of the Name key to the GenericName key. Note that a desktop file requires a Name key to be valid, so this option will always have an effect."
);
QCommandLineOption setGenericNameOption(
    "set-generic-name",
    "Set the generic name (key DisplayName) to GENERIC-NAME. If a generic name was already set, it will be overridden. Localizations of the old generic name will be removed.",
    "GENERIC-NAME"
);
QCommandLineOption copyGenericNameToNameOption(
    "copy-generic-name-to-name",
    "NOT IMPLEMENTED"
);
QCommandLineOption setCommentOption(
    "set-comment",
    "NOT IMPLEMENTED",
    "COMMENT"
);
QCommandLineOption setIconOption(
    "set-icon",
    "Set the icon (key Icon) to ICON. If an icon was already set, it will be overridden. Localizations of the old icon will be removed.",
    "ICON"
);
QCommandLineOption addCategoryOption(
    "add-category",
    "NOT IMPLEMENTED",
    "CATEGORY"
);
QCommandLineOption removeCategoryOption(
    "remove-category",
    "NOT IMPLEMENTED",
    "CATEGORY"
);
QCommandLineOption addMimeTypeOption(
    "add-mime-type",
    "NOT IMPLEMENTED",
    "MIME-TYPE"
);
QCommandLineOption removeMimeTypeOption(
    "remove-mime-type",
    "NOT IMPLEMENTED",
    "MIME-TYPE"
);
QCommandLineOption addOnlyShowInOption(
    "add-only-show-in",
    "NOT IMPLEMENTED",
    "ENVIRONMENT"
);
QCommandLineOption removeOnlyShowInOption(
    "remove-only-show-in",
    "NOT IMPLEMENTED",
    "ENVIRONMENT"
);
QCommandLineOption addNotShowInOption(
    "add-not-show-in",
    "NOT IMPLEMENTED",
    "ENVIRONMENT"
);
QCommandLineOption removeNotShowInOption(
    "remove-not-show-in",
    "NOT IMPLEMENTED",
    "ENVIRONMENT"
);
QCommandLineOption removeKeyOption(
    "remove-key",
    "Remove the KEY key from the desktop files, if present.",
    "KEY"
);

void addEditOptions(QCommandLineParser& parser){
    parser.addOption(setKeyOption);
    parser.addOption(setValueOption);
    parser.addOption(setNameOption);
    parser.addOption(copyNameToGenericNameOption);
    parser.addOption(setGenericNameOption);
    parser.addOption(copyGenericNameToNameOption);
    parser.addOption(setCommentOption);
    parser.addOption(setIconOption);
    parser.addOption(addCategoryOption);
    parser.addOption(removeCategoryOption);
    parser.addOption(addMimeTypeOption);
    parser.addOption(removeMimeTypeOption);
    parser.addOption(addOnlyShowInOption);
    parser.addOption(removeOnlyShowInOption);
    parser.addOption(addNotShowInOption);
    parser.addOption(removeNotShowInOption);
    parser.addOption(removeKeyOption);
}

bool validateSetKeyValueOptions(QCommandLineParser& parser){
    auto options = parser.optionNames();
    if(parser.isSet(setKeyOption) || parser.isSet(setValueOption)){
        for(int i=0; i< options.length(); i++){
            auto option = options[i];
            if(setValueOption.names().contains(option)){
                qDebug() << "Option \"--set-value\" used without a prior \"--set-key\" option";
                qDebug() << "Run 'desktop-file-edit --help' to see a full list of available command line options.";
                return false;
            }
            if(setKeyOption.names().contains(option)){
                option = options[++i];
                if(!setValueOption.names().contains(option)){
                    qDebug() << "Option \"--set-key\" used without a following \"--set-value\" option";
                    qDebug() << "Run 'desktop-file-edit --help' to see a full list of available command line options.";
                    return false;
                }
            }
        }
    }
    return true;
}

void applyChanges(QCommandLineParser& parser, QJsonObject& reg, const QString& name){
    int removeIndex = 0;
    int keyIndex = 0;
    int genericNameIndex = 0;
    int iconIndex = 0;
    QString key;
    auto options = parser.optionNames();
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
}
