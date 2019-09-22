#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include "controller.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

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
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller controller;
    qmlRegisterType<AppItem>();
    qmlRegisterType<Controller>();
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("apps", QVariant::fromValue(controller.getApps()));
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    QQuickItem* appsView = root->findChild<QQuickItem*>("appsView");
    if(!appsView){
        qDebug() << "Can't find appsView";
        return -1;
    }
//    QObject::connect(root, SIGNAL(quit()), &app, SLOT(quit()));
    return app.exec();
}
