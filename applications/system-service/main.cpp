#include <QCommandLineParser>
#include <QGuiApplication>

#include <cstdlib>

#include "dbusservice.h"
#include "signalhandler.h"

using namespace std;

const char *qt_version = qVersion();

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}

int main(int argc, char *argv[]){
    initSentry("tarnish", argv);
    if(deviceSettings.getDeviceType() == DeviceSettings::RM2 && getenv("RM2FB_ACTIVE") == nullptr){
#ifdef SENTRY
        sentry_breadcrumb("error", "rm2fb not detected.");
#endif
        qWarning() << "rm2fb not detected. Running xochitl instead!";
        return QProcess::execute("/usr/bin/xochitl", QStringList());
    }
    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
#endif
    QGuiApplication app(argc, argv);
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

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    dbusService;
    QTimer::singleShot(0, []{
        dbusService->startup();
    });
    system("mkdir -p /run/oxide");
    system(("echo " + to_string(app.applicationPid()) + " > /run/oxide/oxide.pid").c_str());
    QObject::connect(&app, &QGuiApplication::aboutToQuit, []{
        remove("/run/oxide/oxide.pid");
    });
    return app.exec();
}
