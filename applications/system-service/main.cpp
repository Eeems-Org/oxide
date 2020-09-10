#include <QCommandLineParser>

#include <cstdlib>

#include "dbusservice.h"
#include "buttonhandler.h"

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872
#define TEMP_USE_REMARKABLE_DRAW 0x0018

using namespace std;

QString oxidepid;

void signalHandler(__attribute__((unused)) const int signum){
    exit(EXIT_FAILURE);
}

void onExit(){
    auto i_ppid = stoi(oxidepid.toStdString());
    string my_pid = to_string(getpid());
    auto procs  = ButtonHandler::split_string_by_newline(ButtonHandler::exec((
        "grep -Erl /proc/*/status --regexp='PPid:\\s+" + oxidepid + "' | awk '{print substr($1, 7, length($1) - 13)}'"
    ).toStdString().c_str()));
    qDebug() << "Pausing child tasks...";
    for(auto pid : procs){
      string cmd = "cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'";
      if(my_pid != pid && ButtonHandler::is_uint(pid) && ButtonHandler::exec(cmd.c_str()) == oxidepid.toStdString() + "\n"){
          qDebug() << "  " << pid.c_str();
          // Found a child process
          auto i_pid = stoi(pid);
          // Pause the process
          kill(i_pid, SIGSTOP);
      }
    }
    kill(i_ppid, SIGSTOP);
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    app.setApplicationName("rot");
    app.setApplicationVersion("1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide system service");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    parser.addPositionalArgument("pid", "Oxide PID");

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(args.length() < 1){
        parser.showHelp(EXIT_FAILURE);
    }
    oxidepid = args.at(0);

    // DBus API service
    DBusService::singleton();
    // Button handling
    ButtonHandler::init(oxidepid);
    signal(SIGTERM, signalHandler);
    atexit(onExit);
    return app.exec();
}
