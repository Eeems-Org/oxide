#include "autotest.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QDebug>
#include <libblight.h>
#include <libblight/dbus.h>
#include "test.h"

int wait_for_service(QProcess* blight, QCoreApplication* app){
    qDebug() << "Checking to see if blight is running...";
    std::unique_ptr<Blight::DBus> dbus = std::make_unique<Blight::DBus>(
#ifdef EPAPER
      true
#else
      false
#endif
    );
    if(!dbus->has_service(BLIGHT_SERVICE)){
        blight->start("/opt/bin/blight", QStringList());
        qDebug() << "Waiting for blight to start...";
        if(!blight->waitForStarted()){
            qDebug() << "Failed to start: " << blight->exitCode();
            return 1;
        }
        qDebug() << "Waiting for blight dbus service...";
        while(!dbus->has_service(BLIGHT_SERVICE)){
            app->processEvents(QEventLoop::AllEvents, 1000);
            if(blight->state() == QProcess::NotRunning){
                qDebug() << "Blight failed to start:" << blight->exitCode();
                return blight->exitCode() || 1;
            }
        }
    }
    return 0;
}

int run_c_tests(QCoreApplication* app){
    QProcess blight;
    int res = wait_for_service(&blight, app);
    if(res){
        return res;
    }
    res = test_c();
    if(blight.state() != QProcess::NotRunning){
        qDebug() << "Waiting for blight to stop...";
        blight.kill();
        blight.waitForFinished();
    }
    return res;
}

int main(int argc, char* argv[]){
    QThread::currentThread()->setObjectName("main");
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTimer::singleShot(0, [&app, &argc, &argv]{
        int res = 0;
        if(getenv("SKIP_C_TESTS") == nullptr){
            res = run_c_tests(&app);
        }
        res += AutoTest::run(argc, argv);
        app.exit(res);
    });
    return app.exec();
}
