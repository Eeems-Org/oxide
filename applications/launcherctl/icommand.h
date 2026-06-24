#pragma once

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtPreprocessorSupport>

#define O_COMMAND_BODY(_class, _name, _help, _allowEmpty)                      \
public:                                                                        \
  _class()                                                                     \
    : ICommand(_allowEmpty) {                                                  \
    if (commands->contains(_name)) {                                           \
      throw "ICommand " _name " already exists";                               \
    }                                                                          \
    commands->insert(                                                          \
      _name,                                                                   \
      Command{                                                                 \
        .help = _help,                                                         \
        .command = this,                                                       \
      }                                                                        \
    );                                                                         \
  }
#define O_COMMAND_X_1(_class, _name, _help)                                    \
  O_COMMAND_BODY(_class, _name, _help, false)
#define O_COMMAND_X_2(_class, _name, _help, _allowEmpty)                       \
  O_COMMAND_BODY(_class, _name, _help, _allowEmpty)
#define O_COMMAND_X_get_func(arg1, arg2, arg3, arg4, arg5, ...) arg5
#define O_COMMAND_X(...)                                                       \
  O_COMMAND_X_get_func(__VA_ARGS__, O_COMMAND_X_2, O_COMMAND_X_1, )

#define O_COMMAND(...) O_COMMAND_X(__VA_ARGS__)(__VA_ARGS__)
#define STATIC_INSTANCE(_class) Q_UNUSED(new _class());

class ICommand;

struct Command {
  QString help;
  ICommand* command;
};

class ICommand {
public:
  static QMap<QString, Command>* commands;

  static QString commandsHelp();
  static int exec(QCommandLineParser& parser);
  static int exec(const QStringList& args);

  ICommand(bool allowEmpty);
  virtual int arguments(QCommandLineParser& parser) {
    Q_UNUSED(parser);
    return EXIT_FAILURE;
  }
  virtual int command(QCommandLineParser& parser, const QStringList& args) {
    Q_UNUSED(parser);
    Q_UNUSED(args);
    return EXIT_FAILURE;
  }

protected:
  bool allowEmpty = false;
};
