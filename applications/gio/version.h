#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>

class VersionCommand : ICommand{
    O_COMMAND(VersionCommand, "version", "Print the version of Oxide that gio belongs to", true)
    int arguments() override{ return EXIT_SUCCESS; }
    int command(const QStringList& args) override{
        if(!args.isEmpty()){
            parser->showHelp(EXIT_FAILURE);
        }
        parser->showVersion();
        return EXIT_SUCCESS;
    }
};
