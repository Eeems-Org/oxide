#include <QCommandLineParser>
#include <QFile>

#include <errno.h>
#include <linux/input.h>
#include <liboxide.h>
#include <liboxide/event_device.h>

using namespace Oxide;
using namespace Oxide::Sentry;

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("inject_evdev", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("inject_evdev");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Inject evdev events.");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("device", "Device to emit events to. See /sys/input/class for possible values.");
    parser.process(app);
    QStringList args = parser.positionalArguments();
    if (args.isEmpty() || args.length() > 1) {
        parser.showHelp(EXIT_FAILURE);
    }
    auto name = args.first();
    QString path = QString("/sys/class/input/%1").arg(name);
    if(!QFile::exists(path)){
        qDebug() << "Device does not exist:" << name.toStdString().c_str();
        return EXIT_FAILURE;
    }
    event_device device(path.toStdString(), O_RDWR);
    if(device.error > 0){
        qDebug() << "Failed to open device:" << name.toStdString().c_str();
        qDebug() << strerror(device.error);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
