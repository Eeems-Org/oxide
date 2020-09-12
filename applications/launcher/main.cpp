#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <QQuickItem>
#include <QObject>
#include <QMap>
#include <QSettings>
#include <QProcess>
#include <QStringList>
#include <QtDBus>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>

#include "controller.h"
#include "eventfilter.h"
#include "wifimanager.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

using namespace std;

const char *qt_version = qVersion();
QProcess* tarnishProc = nullptr;

vector<std::string> split_string_by_newline(const std::string& str){
    auto result = vector<std::string>{};
    auto ss = stringstream{str};

    for (string line; getline(ss, line, '\n');)
        result.push_back(line);

    return result;
}
int is_uint(string input){
    unsigned int i;
    for (i=0; i < input.length(); i++){
        if(!isdigit(input.at(i))){
            return 0;
        }
    }
    return 1;
}
void signalHandler(__attribute__((unused)) const int signum){
    exit(EXIT_SUCCESS);
}
void onExit(){
    if(tarnishProc != nullptr && tarnishProc->processId()){
        qDebug() << "Killing tarnish";
        tarnishProc->kill();
    }
    auto ppid = to_string(getpid());
    auto procs  = split_string_by_newline(Controller::exec((
        "grep -Erl /proc/*/status --regexp='PPid:\\s+" + ppid + "' | awk '{print substr($1, 7, length($1) - 13)}'"
    ).c_str()));
    qDebug() << "Killing child tasks...";
    for(auto pid : procs){
      string cmd = "cat /proc/" + pid + "/status | grep PPid: | awk '{print$2}'";
      if(is_uint(pid) && Controller::exec(cmd.c_str()) == ppid + "\n"){
          qDebug() << "  " << pid.c_str();
          // Found a child process
          auto i_pid = stoi(pid);
          // Pause the process
          kill(i_pid, SIGTERM);
      }
    }
}

int main(int argc, char *argv[]){
    signal(SIGTERM, signalHandler);
    std::atexit(onExit);
//    QSettings xochitlSettings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat);
//    xochitlSettings.sync();
//    qDebug() << xochitlSettings.value("Password").toString();

    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "rotate=180");
    qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
//    qputenv("QT_DEBUG_BACKINGSTORE", "1");
#endif
    QProcess::execute("killall", QStringList() << "tarnish" << "erode");
    tarnishProc = new QProcess();
    tarnishProc->setProcessChannelMode(QProcess::ForwardedChannels);
    tarnishProc->setArguments(QStringList() << std::to_string(getpid()).c_str());
    qDebug() << "Looking for tarnish";
    for(auto tarnish : QList<QString> { "/opt/bin/tarnish", "/bin/tarnish", "/home/root/.local/tarnish"}){
        qDebug() << "  " << tarnish.toStdString().c_str();
        if(QFile(tarnish).exists()){
            qDebug() << "   Found";
            tarnishProc->setProgram(tarnish);
            tarnishProc->start();
            tarnishProc->waitForStarted();
            break;
        }
    }
    if(!tarnishProc->processId()){
        qDebug() << "tarnish not found or is running";
    }
    WifiManager::singleton();
    QGuiApplication app(argc, argv);
    EventFilter filter;
    app.installEventFilter(&filter);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller* controller = new Controller();
    controller->killXochitl();
    controller->filter = &filter;
    qmlRegisterType<AppItem>();
    qmlRegisterType<Controller>();
    controller->loadSettings();
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("apps", QVariant::fromValue(controller->getApps()));
    context->setContextProperty("controller", controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    controller->root = root;
    filter.root = (QQuickItem*)root;
    QObject* stateController = root->findChild<QObject*>("stateController");
    if(!stateController){
        qDebug() << "Can't find stateController";
        return -1;
    }
    QObject* clock = root->findChild<QObject*>("clock");
    if(!clock){
        qDebug() << "Can't find clock";
        return -1;
    }
    // Update UI
    clock->setProperty("text", QTime::currentTime().toString("h:mm a"));
    controller->updateUIElements();
    // Setup suspend timer
    filter.timer = new QTimer(root);
    filter.timer->setInterval(5 * 60 * 1000); // 5 minutes
    QObject::connect(filter.timer, &QTimer::timeout, [stateController, controller](){
        if(!controller->getPowerConnected()){
            qDebug() << "Suspending due to inactivity...";
            stateController->setProperty("state", QString("suspended"));
        }
    });
    // Setup clock
    QTimer* clockTimer = new QTimer(root);
    auto currentTime = QTime::currentTime();
    QTime nextTime = currentTime.addSecs(60 - currentTime.second());
    clockTimer->setInterval(currentTime.msecsTo(nextTime)); // nearest minute
    QObject::connect(clockTimer , &QTimer::timeout, [clock, &clockTimer](){
        clock->setProperty("text", QTime::currentTime().toString("h:mm a"));
        if(clockTimer->interval() != 60 * 1000){
            clockTimer->setInterval(60 * 1000); // 1 minute
        }
    });
    clockTimer ->start();
    if(controller->automaticSleep()){
        filter.timer->start();
    }
    return app.exec();
}
