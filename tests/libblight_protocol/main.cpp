#include "autotest.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QDebug>
#include <libblight.h>
#include <libblight/dbus.h>
#include "test.h"

int main(int argc, char* argv[]){
    QThread::currentThread()->setObjectName("main");
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTimer::singleShot(0, [&app, argc, argv]{
        QProcess blight;
        qDebug() << "Checking to see if blight is running...";
        auto dbus = new Blight::DBus(
#ifdef EPAPER
          true
#else
          false
#endif
        );
        if(!dbus->has_service(BLIGHT_SERVICE)){
            blight.setEnvironment(QStringList() << "RM2FB_ACTIVE=1");
            blight.start("/opt/bin/blight", QStringList());
            qDebug() << "Waiting for blight to start...";
            if(!blight.waitForStarted()){
                qDebug() << "Failed to start: " << blight.exitCode();
                app.exit(1);
            }
            qDebug() << "Waiting for blight dbus service...";
            while(!dbus->has_service(BLIGHT_SERVICE)){
                app.processEvents(QEventLoop::AllEvents, 1000);
                if(blight.state() == QProcess::NotRunning){
                    qDebug() << "Blight failed to start:" << blight.exitCode();
                    app.exit(blight.exitCode() || 1);
                    return;
                }
            }
        }
        delete dbus;
        test_c();
        int res = AutoTest::run(argc, argv);
        if(blight.state() != QProcess::NotRunning){
            qDebug() << "Waiting for blight to stop...";
            blight.kill();
            blight.waitForFinished();
        }
        app.exit(res);
    });
    return app.exec();
}
