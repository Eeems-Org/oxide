#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>

class HelpCommand : ICommand{
    O_COMMAND(HelpCommand, "help", "Print help", true)
    int arguments() override{
        parser->addPositionalArgument("Commands:", commandsHelp(), "[COMMAND]");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        if(args.isEmpty()){
            parser->showHelp(EXIT_SUCCESS);
        }
        auto command = args.first();
        auto tempArgs = QStringList() << command;
        if(!commands->contains(command)){
            parser->showHelp(EXIT_FAILURE);
        }
        parser->clearPositionalArguments();
        parser->addPositionalArgument("", "", command);
        auto res = commands->value(command).command->arguments();
        parser->process(tempArgs);
        parser->showHelp(res);
        return res;
    }
};
