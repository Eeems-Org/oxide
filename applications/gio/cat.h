#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>

class CatCommand : ICommand{
    O_COMMAND(CatCommand, "cat", "Concatenates the given files and prints them to the standard output.")
    int arguments() override{
        parser->addPositionalArgument("location", "Locations to concatenate", "LOCATION...");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        for(const auto& path : args){
            auto url = urlFromPath(path);
            if(!url.isLocalFile()){
                GIO_ERROR(url, path, "Error while parsing path", "Path is not a local file");
                continue;
            }
            QFile file(path);
            if(!file.exists()){
                GIO_ERROR(url, path, "Error when getting information for file", "No such file or directory");
                continue;
            }
            if(!file.open(QFile::ReadOnly)){
                GIO_ERROR(url, path, "Error opening file", "Permission denied");
                continue;
            }
            while(!file.atEnd()){
                qStdOut() << file.read(1024);
            }
            file.close();
        }
        return EXIT_SUCCESS;
    }
};
