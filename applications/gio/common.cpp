#include "common.h"

QTextStream& qStdOut(){
    static QTextStream ts( stdout );
    return ts;
}

static auto _commands = new QMap<QString, Command>;

QMap<QString, Command>* ICommand::commands(){ return _commands; }

QCommandLineOption ICommand::versionOption(){
    static QCommandLineOption value(
        {"v", "version"},
        "Displays version information."
    );
    return value;
}

QString ICommand::commandsHelp(){
    QString value;
    for(const auto& name : _commands->keys()){
        const auto& item = _commands->value(name);
        value += QString("%1\t%2\n").arg(name, item.help);
    }
    return value;
}
int ICommand::exec(QCommandLineParser& parser){
    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(EXIT_FAILURE);
    }
    if(parser.isSet(ICommand::versionOption())){
        parser.showVersion();
    }
    auto name = args.first().toStdString().c_str();
    if(!_commands->contains(name)){
        parser.showHelp(EXIT_FAILURE);
    }
    auto command = _commands->value(name).command;
    auto res = command->arguments(parser);
    if(res){
        return res;
    }
    parser.process(*qApp);
    if(parser.isSet(versionOption())){
        parser.showHelp(EXIT_FAILURE);
    }
    args = parser.positionalArguments();
    args.removeFirst();
    if(!command->allowEmpty && args.isEmpty()){
        parser.showHelp(EXIT_FAILURE);
    }
    return command->command(args);
}

ICommand::ICommand(bool allowEmpty) : allowEmpty(allowEmpty){}
