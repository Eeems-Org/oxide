#include "common.h"

QTextStream& qStdOut(){
    static QTextStream ts( stdout );
    return ts;
}

QMap<QString, Command>* ICommand::commands = new QMap<QString, Command>;
QCommandLineParser* ICommand::parser = nullptr;

QCommandLineOption ICommand::versionOption(){
    static QCommandLineOption value(
        {"v", "version"},
        "Displays version information."
    );
    return value;
}

QString ICommand::commandsHelp(){
    QString value;
    for(const auto& name : commands->keys()){
        const auto& item = commands->value(name);
        value += QString("%1\t%2\n").arg(name, item.help);
    }
    return value;
}
int ICommand::exec(QCommandLineParser& _parser){
    parser = &_parser;
    QStringList args = _parser.positionalArguments();
    if (args.isEmpty()) {
        _parser.showHelp(EXIT_FAILURE);
    }
    if(_parser.isSet(ICommand::versionOption())){
        _parser.showVersion();
    }
    auto name = args.first();
    if(!commands->contains(name)){
        _parser.showHelp(EXIT_FAILURE);
    }
    auto command = commands->value(name).command;
    auto res = command->arguments();
    if(res){
        return res;
    }
    _parser.process(*qApp);
    if(_parser.isSet(versionOption())){
        _parser.showHelp(EXIT_FAILURE);
    }
    args = _parser.positionalArguments();
    args.removeFirst();
    if(!command->allowEmpty && args.isEmpty()){
        _parser.showHelp(EXIT_FAILURE);
    }
    return command->command(args);
}

ICommand::ICommand(bool allowEmpty) : allowEmpty(allowEmpty){}
