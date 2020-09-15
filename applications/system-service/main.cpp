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
    exit(EXIT_SUCCESS);
}

void onExit(){
    DBusService::shutdown();
    string my_pid = to_string(getpid());
    auto procs  = ButtonHandler::split_string_by_newline(ButtonHandler::exec((
        "grep -Erl /proc/*/status --regexp='PPid:\\s+" + my_pid + "' | awk '{print substr($1, 7, length($1) - 13)}'"
    ).c_str()));
    qDebug() << "Killing child tasks...";
    for(auto pid : procs){
        string cmd = "cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'";
        if(my_pid != pid && ButtonHandler::is_uint(pid) && ButtonHandler::exec(cmd.c_str()) == my_pid + "\n"){
            qDebug() << "  " << pid.c_str();
            // Found a child process
            auto i_pid = stoi(pid);
            // Kill the process
            kill(i_pid, SIGTERM);
        }
    }
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("tarnish");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide system service");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    parser.addPositionalArgument("pid", "Oxide PID");

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(args.length() > 0){
        oxidepid = args.at(0);
    }else if(!system("systemctl --quiet is-active oxide")){
        oxidepid = ButtonHandler::exec("systemctl status oxide | grep 'Main PID:' | awk '{print $3}'").c_str();
        oxidepid = oxidepid.trimmed();
        if(!oxidepid.isEmpty()){
            qDebug() << "Automatically detected oxide with PID: " << oxidepid;
        }
    }else{
        qDebug() << "Oxide not detected. Assuming running standalone";
        oxidepid = "";
    }

    // DBus API service
    DBusService::singleton();
    signal(SIGTERM, signalHandler);
    atexit(onExit);
    // Button handling
    ButtonHandler::init(oxidepid);
    return app.exec();
}
