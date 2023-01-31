#pragma once

#include "common.h"

class Cat : ICommand{
    O_COMMAND(Cat, "cat", "Concatenates the given files and prints them to the standard output.")
protected:
    int command(const QStringList& args);
    int arguments(QCommandLineParser& parser);
};
