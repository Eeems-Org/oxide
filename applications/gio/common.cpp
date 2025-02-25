#include "common.h"

#include <QUrl>
#include <QDir>

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

#define FULL_SIZE 66

QString ICommand::commandsHelp(){
    QString value;
    const QList<QString>& keys = commands->keys();
    int leftSize = (*std::max_element(keys.constBegin(), keys.constEnd(), [](const QString& v1, const QString& v2){
        return v1.length() < v2.length();
    })).length() + 2;
    int rightSize = FULL_SIZE - leftSize - 1;
    for(const auto& name : keys){
        const auto& item = commands->value(name);
        value += "\n" + name.leftJustified(leftSize, ' ');
        if(item.help > QChar(rightSize)){
            QStringList words = item.help.split(' ');
            QString padding = QString().leftJustified(leftSize + 1, ' ');
            bool first = true;
            while(!words.isEmpty()){
                QString strPart;
                while(!words.isEmpty()){
                    auto word = words.first();
                    // If the first word is larger than a single line
                    if(!strPart.length() && word.length() > rightSize){
                        // Fill the line with what you can
                        strPart = word.left(rightSize);
                        words.replace(0, word.mid(rightSize));
                        // Exit since we have what we need to display
                        break;
                    }
                    // Exit if the next word would be too long
                    if(strPart.length() + 1 + word.length() > rightSize){
                        break;
                    }
                    // Add the word to the results
                    strPart += word + " ";
                    words.removeFirst();
                }
                // Don't padd the first line, it doesn't need it
                if(first){
                    first = false;
                }else{
                    value += "\n";
                    value += padding;
                }
                value += strPart;
            }
        }else{
            value += item.help;
        }
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
    parser->clearPositionalArguments();
    parser->addPositionalArgument("", "", name);
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
QUrl ICommand::urlFromPath(const QString& path){
    auto url = QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
    if(url.scheme().isEmpty()){
        url.setScheme("file");
    }
    return url;
}
