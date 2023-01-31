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
            auto url = QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
            if(url.scheme().isEmpty()){
                url.setScheme("file");
            }
            if(!url.isLocalFile()){
                GIO_ERROR(url, path, "No such file or directory");
                continue;
            }
            QFile file(path);
            if(!file.open(QFile::ReadOnly)){
                GIO_ERROR(url, path, "Permission denied");
                continue;
            }
            while(!file.atEnd()){
                qStdOut() << file.read(1);
            }
            file.close();
        }
        return EXIT_SUCCESS;
    }
};
