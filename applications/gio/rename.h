#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>
#include <QCommandLineOption>

// [OPTION...] SOURCE... DESTINATION
class RenameCommand : ICommand{
    O_COMMAND(
        RenameCommand, "rename",
        "Renames a file."
    )
    int arguments() override{
        parser->addPositionalArgument("LOCATION", "The location to rename.");
        parser->addPositionalArgument("NAME", "The new name.");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        if(args.length() < 2){
            parser->showHelp(EXIT_FAILURE);
        }
        auto name = args.last();
        if(name.contains("/")){
            qDebug() << "gio: File names cannot contain \"/\"";
            return EXIT_FAILURE;
        }
        auto path = args.first();
        auto url = urlFromPath(path);
        if(!url.isLocalFile()){
            GIO_ERROR(url, path, "Error while parsing path", "Path is not a local file");
            return EXIT_FAILURE;
        }
        QFileInfo info(path);
        if(!info.exists()){
            GIO_ERROR(url, path, "Error renaming file", "No such file or directory");
            return EXIT_FAILURE;
        }
        auto toPath = info.absoluteDir().path() + "/" + name;
        if(!QFile::rename(path, toPath)){
            GIO_ERROR(url, path, "Error renaming file", "Rename failed.");
            return EXIT_FAILURE;
        }
        qDebug() << "Rename successful. New uri:" << urlFromPath(toPath).toString().toStdString().c_str();
        return EXIT_SUCCESS;
    }
};
