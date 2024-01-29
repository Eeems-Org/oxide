#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLockFile>
#include <QWindow>
#include <QScreen>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdlib>
#include <filesystem>
#include <liboxide.h>
#include <liboxide/oxideqml.h>
#include <libblight.h>
#include <libblight/connection.h>

#include "dbusservice.h"
#include "controller.h"

using namespace std;
using namespace Oxide::Sentry;
using namespace Oxide::QML;

const std::string runPath = "/run/oxide";
const char* pidPath = "/run/oxide/oxide.pid";
const char* lockPath = "/run/oxide/oxide.lock";

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}
bool stopProcess(pid_t pid){
    if(pid <= 1){
        return false;
    }
    qDebug() << "Waiting for other instance to stop...";
    kill(pid, SIGTERM);
    int tries = 0;
    while(0 == kill(pid, 0)){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(++tries == 50){
            qDebug() << "Instance is taking too long, killing...";
            kill(pid, SIGKILL);
        }else if(tries == 60){
            qDebug() << "Unable to kill process";
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]){
#ifdef EPAPER
    auto connected = Blight::connect(true);
#else
    auto connected = Blight::connect(false);
#endif
    if(!connected){
        // TODO - attempt to start display server instance
        bool enabled = Oxide::debugEnabled();
        if(!enabled){
            qputenv("DEBUG", "1");
        }
        O_WARNING("Display server not available. Running xochitl instead!");
        if(!enabled){
            qputenv("DEBUG", "0");
        }
        return QProcess::execute("/usr/bin/xochitl", QStringList());
    }
    Blight::connection()->onDisconnect([](int res){
        // TODO - attempt to reconnect
        if(res){
            qApp->exit(res);
        }
    });
    qputenv("XDG_CURRENT_DESKTOP", "OXIDE");
    QThread::currentThread()->setObjectName("main");
    qputenv("QMLSCENE_DEVICE", "software");
    qputenv("QT_QUICK_BACKEND","software");
    qputenv("QT_QPA_PLATFORM", "oxide:enable_fonts");
    QCoreApplication::addLibraryPath("/opt/usr/lib/plugins");
    QGuiApplication app(argc, argv);
    sentry_init("tarnish", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("tarnish");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide system service");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    QCommandLineOption breakLockOption(
        {"f", "break-lock"},
        "Break existing locks and force startup if another version of tarnish is already running"
    );
    parser.addOption(breakLockOption);
    parser.process(app);
    const QStringList args = parser.positionalArguments();
    if(!args.isEmpty()){
        parser.showHelp(EXIT_FAILURE);
    }
    auto actualPid = QString::number(app.applicationPid());
    QString pid = Oxide::execute(
        "systemctl",
        QStringList()
            << "--no-pager"
            << "show"
            << "--property"
            << "MainPID"
            << "--value"
            << "tarnish",
        false
    ).trimmed();
    if(pid != "0" && pid != actualPid){
        if(!parser.isSet(breakLockOption)){
            qDebug() << "tarnish.service is already running";
            return EXIT_FAILURE;
        }
        if(QProcess::execute("systemctl", QStringList() << "stop" << "tarnish")){
            qDebug() << "tarnish.service is already running";
            qDebug() << "Unable to stop service";
            return EXIT_FAILURE;
        }
    }
    qDebug() << "Creating lock file" << lockPath;
    if(!QFile::exists(QString::fromStdString(runPath)) && !std::filesystem::create_directories(runPath)){
        qDebug() << "Failed to create" << runPath.c_str();
        return EXIT_FAILURE;
    }
    int lock = Oxide::tryGetLock(lockPath);
    if(lock < 0){
        qDebug() << "Unable to establish lock on" << lockPath << strerror(errno);
        if(!parser.isSet(breakLockOption)){
            return EXIT_FAILURE;
        }
        qDebug() << "Attempting to stop all other instances of tarnish" << lockPath;
        for(auto lockingPid : Oxide::lsof(lockPath)){
            if(Oxide::processExists(lockingPid)){
                stopProcess(lockingPid);
            }
        }
        lock = Oxide::tryGetLock(lockPath);
        if(lock < 0){
            qDebug() << "Unable to establish lock on" << lockPath << strerror(errno);
            return EXIT_FAILURE;
        }
    }

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [lock]{
        qDebug() << "Releasing lock " << lockPath;
        Oxide::releaseLock(lock, lockPath);
    });

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    dbusService;
    QTimer::singleShot(0, []{
        dbusService->startup();
    });
    QFile pidFile(pidPath);
    if(!pidFile.open(QFile::ReadWrite)){
        qWarning() << "Unable to create " << pidPath;
        return app.exec();
    }
    pidFile.seek(0);
    pidFile.write(actualPid.toUtf8());
    pidFile.close();
    QObject::connect(&app, &QGuiApplication::aboutToQuit, []{
        remove(pidPath);
    });
    QQmlApplicationEngine engine;
    registerQML(&engine);
    QQmlContext* context = engine.rootContext();
    Controller controller(&app);
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if(engine.rootObjects().isEmpty()){
        qFatal("Failed to load main layout");
    }
    return app.exec();
}
