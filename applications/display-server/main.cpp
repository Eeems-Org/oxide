#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLockFile>
#include <QWindow>
#include <QScreen>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdlib>
#include <signal.h>
#include <filesystem>
#include <liboxide.h>

#ifdef EPAPER
#include <QtPlugin>
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

#include "dbusinterface.h"
#include "evdevhandler.h"

using namespace std;
using namespace Oxide::Sentry;

const std::string runPath = "/run/oxide";
const char* pidPath = "/run/oxide/blight.pid";
const char* lockPath = "/run/oxide/blight.lock";

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

#ifdef DEBUG
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <cxxabi.h>

static struct sigaction oldSigSegv;
static void sigsegv_handler(int sig, siginfo_t* info, void* context){
    unw_cursor_t cursor;
    unw_context_t ucontext;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&ucontext);
    unw_init_local(&cursor, &ucontext);

    // Unwind frames one by one, going up the frame stack.
    while(unw_step(&cursor) > 0){
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if(pc == 0){
            break;
        }
        std::printf("0x%lx:", pc);

        char sym[256];
        if(unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0){
            char* nameptr = sym;
            int status;
            char* demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);
            if (status == 0) {
                nameptr = demangled;
            }
            std::printf(" (%s+0x%lx)\n", nameptr, offset);
            std::free(demangled);
        } else {
            std::printf(" -- error: unable to obtain symbol name for this frame\n");
        }
    }
    oldSigSegv.sa_sigaction(sig, info, context);
    return;
}
#endif

int main(int argc, char* argv[]){
#ifdef __arm__
    if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2){
        // TODO - also detect if rm2fb-server is running and start it if it isn't
        if(getenv("RM2FB_ACTIVE") == nullptr){
            bool enabled = Oxide::debugEnabled();
            if(!enabled){
                qputenv("DEBUG", "1");
            }
            O_WARNING("rm2fb not detected. Running xochitl instead!");
            execl("/usr/bin/xochitl", "/usr/bin/xochitl", NULL);
            O_WARNING("Failed to run xochitl: " << std::strerror(errno));
            return errno;
        }
    }
#endif
    qputenv("XDG_CURRENT_DESKTOP", "OXIDE");
    QThread::currentThread()->setObjectName("main");
#ifdef EPAPER
    // Force use of offscreen as we'll be using EPFrameBuffer directly
    qputenv("QMLSCENE_DEVICE", "");
    qputenv("QT_QUICK_BACKEND","");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // qputenv("QT_QPA_PLATFORM", "vnc:size=1404x1872");
#endif
    QGuiApplication app(argc, argv);
    sentry_init("blight", argv);
#ifdef DEBUG
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigsegv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, &oldSigSegv);
#endif
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("blight");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide display service");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    QCommandLineOption breakLockOption(
        {"f", "break-lock"},
        "Break existing locks and force startup if another version of blight is already running"
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
        QStringList() << "--no-pager" << "show" << "--property" << "MainPID" << "--value" << "blight"
    ).trimmed();
    if(pid != "0" && pid != "QML debugging is enabled. Only use this in a safe environment.\n0" && pid != actualPid){
        if(!parser.isSet(breakLockOption)){
            qDebug() << "blight.service is already running" << pid;
            return EXIT_FAILURE;
        }
        if(QProcess::execute("systemctl", QStringList() << "stop" << "blight")){
            qDebug() << "blight.service is already running";
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
        qDebug() << "Attempting to stop all other instances of blight" << lockPath;
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
    dbusInterface;
    evdevHandler;
    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGBUS, SIG_IGN);
    return app.exec();
}
