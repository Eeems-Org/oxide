#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include "appsview.h"

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
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QQuickItem* appsView = engine.rootObjects().first()->findChild<QQuickItem*>("appsView");
    if(!appsView){
        qDebug() << "Can't find appsView";
        return -1;
    }
//    QObject::connect((QObject*)engine.rootObjects().first(), SIGNAL(quit()), &app, SLOT(quit()));
    AppsView(&engine, &app, appsView);
    return app.exec();
}
