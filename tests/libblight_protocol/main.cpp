#include "autotest.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QDebug>
#include <libblight.h>
#include <libblight/dbus.h>
#include "test.h"

int run_c_tests(QCoreApplication* app){
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
        blight.start("/opt/bin/blight", QStringList());
        qDebug() << "Waiting for blight to start...";
        if(!blight.waitForStarted()){
            qDebug() << "Failed to start: " << blight.exitCode();
            return 1;
        }
        qDebug() << "Waiting for blight dbus service...";
        while(!dbus->has_service(BLIGHT_SERVICE)){
            app->processEvents(QEventLoop::AllEvents, 1000);
            if(blight.state() == QProcess::NotRunning){
                qDebug() << "Blight failed to start:" << blight.exitCode();
                return blight.exitCode() || 1;
            }
        }
    }
    delete dbus;
    test_c();
    if(blight.state() != QProcess::NotRunning){
        qDebug() << "Waiting for blight to stop...";
        blight.kill();
        blight.waitForFinished();
    }
    return 0;
}

int main(int argc, char* argv[]){
    QThread::currentThread()->setObjectName("main");
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTimer::singleShot(0, [&app, argc, argv]{
        if(getenv("SKIP_C_TESTS") == nullptr){
            int res = run_c_tests(&app);
            if(res){
                app.exit(res);
                return;
            }
        }
        app.exit(AutoTest::run(argc, argv));
    });
    return app.exec();
}
