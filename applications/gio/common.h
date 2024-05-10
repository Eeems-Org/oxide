#pragma once

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMap>
#include <QDebug>
#include <QString>
#include <QTextStream>

QTextStream& qStdOut();

#define GIO_ERROR(url, path, message, error) \
    qDebug() \
        << "gio:" \
        << url.toString().toStdString().c_str() \
        << ": " \
        << message \
        << QFileInfo(path).absoluteFilePath().toStdString().c_str() \
        << ": " \
        << error;

#define O_COMMAND_BODY(_class, _name, _help, _allowEmpty) \
public: \
    _class() : ICommand(_allowEmpty) { \
        if(commands->contains(_name)){ \
            throw "ICommand " _name " already exists"; \
        } \
        commands->insert(_name, Command{ \
            .help = _help, \
            .command = this, \
        }); \
    }
#define O_COMMAND_X_1(_class, _name, _help) O_COMMAND_BODY(_class, _name, _help, false)
#define O_COMMAND_X_2(_class, _name, _help, _allowEmpty) O_COMMAND_BODY(_class, _name, _help, _allowEmpty)
#define O_COMMAND_X_get_func(arg1, arg2, arg3, arg4, arg5, ...) arg5
#define O_COMMAND_X(...) \
    O_COMMAND_X_get_func(__VA_ARGS__, \
        O_COMMAND_X_2, \
        O_COMMAND_X_1, \
    )

#define O_COMMAND(...) O_COMMAND_X(__VA_ARGS__)(__VA_ARGS__)
#define STATIC_INSTANCE(_class) Q_UNUSED(new _class());
#define O_COMMAND_STUB(_name) \
    class _name ## Command : ICommand{ \
        O_COMMAND(_name ## Command, #_name, "NOT IMPLEMENTED") \
        int arguments() override{ \
            qDebug() << "This has not been implemented."; \
            return EXIT_FAILURE; \
        } \
        int command(const QStringList& args) override{ \
            Q_UNUSED(args) \
            qDebug() << "This has not been implemented."; \
            return EXIT_FAILURE; \
        } \
    }; \
    STATIC_INSTANCE(_name ## Command);

class ICommand;

struct Command {
    QString help;
    ICommand* command;
};

class ICommand {
public:
    static QMap<QString, Command>* commands;
    static QCommandLineParser* parser;

    static QCommandLineOption versionOption();
    static QString commandsHelp();
    static int exec(QCommandLineParser& _parser);

    ICommand(bool allowEmpty);
    virtual int arguments(){ return EXIT_FAILURE; }
    virtual int command(const QStringList& args){
        Q_UNUSED(args);
        return EXIT_FAILURE;
    }

protected:
    bool allowEmpty = false;
    QUrl urlFromPath(const QString& path);
};
