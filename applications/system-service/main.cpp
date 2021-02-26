#include <QCommandLineParser>
#include <QGuiApplication>

#include <cstdlib>

#include "rmkit.h"

#include "dbusservice.h"
#include "signalhandler.h"

using namespace std;

const char *qt_version = qVersion();

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}

int main(int argc, char *argv[]){
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
    dbusService;

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    auto gesture = new input::SwipeGesture();
    gesture->fingers = 1;
    gesture->set_coordinates(0, 0.9, 1, 1);
    input::Gesture::Point point;
    point.x = 0;
    point.y = -1;
    gesture->direction = point;
    gesture->events.activate += PLS_LAMBDA(auto d){
        Q_UNUSED(d);
        qDebug() << "Swipe up";
    };
    QTimer timer(&app);
    QObject::connect(&timer, &QTimer::timeout, []{
        ui::MainLoop::handle_events();
        ui::MainLoop::handle_gestures();
    });
    timer.start(0);
    return app.exec();
}
