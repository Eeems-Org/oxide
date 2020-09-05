#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include <QtPlugin>
#include <linux/input.h>
#include <ostream>
#include <fcntl.h>
#include "controller.h"
#include "eventfilter.h"


#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

struct event_device {
    std::string device;
    int fd;
    event_device(std::string path, int flags){
        device = path;
        fd = open(path.c_str(), flags);
    }
};

const event_device touchScreen("/dev/input/touchscreen0", O_RDWR);


int lock_device(event_device evdev){
    qDebug() << "locking " << evdev.device.c_str();
    int result = ioctl(evdev.fd, EVIOCGRAB, 1);
    if(result == EBUSY){
        qWarning() << "Device is busy";
    }else if(result != 0){
        qWarning() << "Unknown error: " << result;
    }else{
        qDebug() << evdev.device.c_str() << " locked";
    }
    return result;
}

int unlock_device(event_device evdev){
    int result = ioctl(evdev.fd, EVIOCGRAB, 0);
    if(result){
        qDebug() << "Failed to unlock " << evdev.device.c_str() << ": " << result;
    }else{
        qDebug() << "Unlocked " << evdev.device.c_str();
    }
    return result;
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
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "rotate=180");
    qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
//    qputenv("QT_DEBUG_BACKINGSTORE", "1");
#endif
    lock_device(touchScreen);
    QGuiApplication app(argc, argv);
    EventFilter filter;
    app.installEventFilter(&filter);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller controller(&engine);
    if(argc > 1){
        controller.protectPid = std::stoi(argv[1]);
    }
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("tasks", QVariant::fromValue(controller.getTasks()));
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    filter.root = (QQuickItem*)root;
    QQuickItem* tasksView = root->findChild<QQuickItem*>("tasksView");
    if(!tasksView){
        qDebug() << "Can't find tasksView";
        return -1;
    }
    unlock_device(touchScreen);
    return app.exec();
}
