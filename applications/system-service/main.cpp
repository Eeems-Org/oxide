#include <QCommandLineParser>

#include <cstdlib>

#include "dbusservice.h"
#include "signalhandler.h"

using namespace std;

void unixSignalHandler(int unused){
    Q_UNUSED(unused);
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
            int status;
            waitpid(i_pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            if(WIFSTOPPED(status)) {
                kill(i_pid, SIGCONT);
                waitpid(i_pid, &status, WCONTINUED);
            }
            kill(i_pid, SIGTERM);
        }
    }
}

const char *qt_version = qVersion();

int main(int argc, char *argv[]){
    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
#endif
    atexit(onExit);
    signal(SIGTERM, unixSignalHandler);
    signal(SIGKILL, unixSignalHandler);
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

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    dbusService;
    return app.exec();
}
