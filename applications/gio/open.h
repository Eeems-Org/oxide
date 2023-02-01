#pragma once

#include "common.h"

#include <QProcess>
#include <QUrl>

class OpenCommand : ICommand{
    O_COMMAND(OpenCommand, "open", "Open file(s) with xdg-open")
    int arguments() override{
        parser->addPositionalArgument("location", "Locations to open", "LOCATION...");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        for(const auto& path : args){
            auto url = QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
            if(url.scheme().isEmpty()){
                url.setScheme("file");
            }
            if(!url.isLocalFile()){
                GIO_ERROR(url, path, "Error while parsing path", "Path is not a local file");
                continue;
            }
            QProcess::execute("xdg-open", QStringList() << url.toString());
        }
        return EXIT_SUCCESS;
    }
};
