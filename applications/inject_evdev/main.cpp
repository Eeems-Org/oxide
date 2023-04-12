#include <QCommandLineParser>
#include <QFile>
#include <QList>
#include <QSocketNotifier>
#include <QDebug>

#include <stdio.h>
#include <iostream>
#include <liboxide.h>
#include <liboxide/event_device.h>

using namespace Oxide;
using namespace Oxide::Sentry;

event_device* device = nullptr;

bool process_syn(const QStringList& args){
    auto code = args.at(1).toStdString();
    if(code == "SYN_REPORT"){
        device->write(EV_SYN, SYN_REPORT, 0);
        return true;
    }
    if(code == "SYN_MT_REPORT"){
        device->write(EV_SYN, SYN_MT_REPORT, 0);
        return true;
    }
    if(code == "SYN_DROPPED"){
        device->write(EV_SYN, SYN_DROPPED, 0);
        return true;
    }
    qDebug() << "Unknown EV_SYN event code:" << code.c_str();
    return false;
}

bool process_abs(const QStringList& args){
    auto code = args.at(1).toStdString();
    int value = 0;
    if(args.count() > 2){
        bool ok;
        QString arg = args.at(2);
        value = arg.toInt(&ok);
        if(!ok){
            qDebug() << "Third argument must be a valid integer:" << args;
            return false;
        }
    }
    if(code == "ABS_MT_POSITION_X"){
        device->write(EV_ABS, ABS_MT_POSITION_X, value);
        return true;
    }
    if(code == "ABS_MT_POSITION_Y"){
        device->write(EV_ABS, ABS_MT_POSITION_Y, value);
        return true;
    }
    if(code == "ABS_MT_TRACKING_ID"){
        device->write(EV_ABS, ABS_MT_TRACKING_ID, value);
        return true;
    }
    if(code == "ABS_MT_SLOT"){
        device->write(EV_ABS, ABS_MT_SLOT, value);
        return true;
    }
    qDebug() << "Unknown EV_ABS event code:" << code.c_str();
    return false;
}

bool process(const QStringList& args){
    if(args.count() < 2 || args.count() > 3){
        qDebug() << "Incorrect number of arguments.";
        if(debugEnabled()){
            qDebug() << "Arguments: " << args.count();
        }
        return false;
    }
    auto type = args.first().toStdString();
    if(type == "EV_SYN"){
        return process_syn(args);
    }
    if(type == "EV_ABS"){
        return process_abs(args);
    }
    qDebug() << "Unknown event type:" << type.c_str();
    return false;
}

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
    parser.addPositionalArgument("device", "Device to emit events to. See /dev/input for possible values.");
    parser.process(app);
    QStringList args = parser.positionalArguments();
    if (args.isEmpty() || args.count() > 1) {
        parser.showHelp(EXIT_FAILURE);
    }
    auto name = args.first();
    QString path = QString("/dev/input/%1").arg(name);
    if(!QFile::exists(path)){
        qDebug() << "Device does not exist:" << name.toStdString().c_str();
        return EXIT_FAILURE;
    }
    device = new event_device(path.toStdString(), O_RDWR);
    if(device->error > 0){
        qDebug() << "Failed to open device:" << name.toStdString().c_str();
        qDebug() << strerror(device->error);
        return EXIT_FAILURE;
    }
    int fd = fileno(stdin);
    bool isStream = !isatty(fd);
    QSocketNotifier notifier(fd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, [&app, &isStream](){
        std::string input;
        std::getline(std::cin, input);
        auto args = QString(input.c_str()).simplified().split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        auto count = args.count();
        if(count && debugEnabled()){
            qDebug() << args;
        }
        if(!isStream && count == 1 && args.first().toLower() == "exit"){
            app.exit(EXIT_SUCCESS);
            return;
        }
        if(count && !process(args) && isStream){
            app.exit(EXIT_FAILURE);
            return;
        }
        if(!isStream){
            std::cout << "> " << std::flush;
        }else if(std::cin.eof()){
            app.exit(EXIT_SUCCESS);
            return;
        }
    });
    if(!isStream){
        qDebug() << "Input is expected in the following format:";
        qDebug() << "  TYPE CODE [VALUE]";
        qDebug() << "";
        qDebug() << "For example:";
        qDebug() << "  ABS_MT_TRACKING_ID -1";
        qDebug() << "  EV_SYN SYN_REPORT";
        qDebug() << "";
        std::cout << "> " << std::flush;
    }
    return app.exec();
}
