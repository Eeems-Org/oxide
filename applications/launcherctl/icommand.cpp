#include "icommand.h"

#include <algorithm>
#include <cstdlib>

QMap<QString, Command>* ICommand::commands = new QMap<QString, Command>;

#define FULL_SIZE 66

QString
ICommand::commandsHelp() {
  QString value;
  const QList<QString>& keys = commands->keys();
  int leftSize = (*std::max_element(
                    keys.constBegin(),
                    keys.constEnd(),
                    [](const QString& v1, const QString& v2) {
                      return v1.length() < v2.length();
                    }
                  ))
                   .length() +
                 2;
  int rightSize = FULL_SIZE - leftSize - 1;
  for (const auto& name : keys) {
    const auto& item = commands->value(name);
    value += "\n" + name.leftJustified(leftSize, ' ');
    if (item.help.size() > rightSize) {
      QStringList words = item.help.split(' ');
      QString padding = QString().leftJustified(leftSize + 1, ' ');
      bool first = true;
      while (!words.isEmpty()) {
        QString strPart;
        while (!words.isEmpty()) {
          auto word = words.first();
          if (!strPart.length() && word.length() > rightSize) {
            strPart = word.left(rightSize);
            words.replace(0, word.mid(rightSize));
            break;
          }
          if (strPart.length() + 1 + word.length() > rightSize) {
            break;
          }
          strPart += word + " ";
          words.removeFirst();
        }
        if (first) {
          first = false;
        } else {
          value += "\n";
          value += padding;
        }
        value += strPart;
      }
    } else {
      value += item.help;
    }
  }
  return value;
}

int
ICommand::exec(QCommandLineParser& parser) {
  QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
    parser.showHelp(EXIT_FAILURE);
  }
  auto name = args.first();
  if (!commands->contains(name)) {
    parser.showHelp(EXIT_FAILURE);
  }
  auto command = commands->value(name).command;
  parser.clearPositionalArguments();
  parser.addPositionalArgument("", "", name);
  auto res = command->arguments(parser);
  if (res) {
    return res;
  }
  parser.process(*qApp);
  args = parser.positionalArguments();
  args.removeFirst();
  if (!command->allowEmpty && args.isEmpty()) {
    parser.showHelp(EXIT_FAILURE);
  }
  return command->command(parser, args);
}

int
ICommand::exec(const QStringList& argv) {
  if (argv.isEmpty()) {
    return EXIT_FAILURE;
  }
  auto name = argv.first();
  if (!commands->contains(name)) {
    return EXIT_FAILURE;
  }
  auto command = commands->value(name).command;
  QCommandLineParser parser;
  parser.setOptionsAfterPositionalArgumentsMode(
    QCommandLineParser::ParseAsPositionalArguments
  );
  parser.addPositionalArgument("", "", name);
  auto res = command->arguments(parser);
  if (res) {
    return res;
  }
  parser.setOptionsAfterPositionalArgumentsMode(
    QCommandLineParser::ParseAsOptions
  );
  parser.process(QStringList{QCoreApplication::applicationFilePath()} + argv);
  QStringList args = parser.positionalArguments();
  args.removeFirst();
  if (!command->allowEmpty && args.isEmpty()) {
    return EXIT_FAILURE;
  }
  return command->command(parser, args);
}

ICommand::ICommand(bool allowEmpty)
  : allowEmpty(allowEmpty) {}
