#include "autotest.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QDebug>
#include <libblight.h>
#include <libblight/dbus.h>
#include "test.h"

/**
 * @brief Ensures that the "blight" service is available.
 *
 * Checks for the presence of the "blight" D-Bus service using a Blight::DBus instance. If the service is not found,
 * starts the "blight" process via the provided QProcess, processes events to allow the service to initialize,
 * and monitors the process state. Returns 0 if the service becomes available, or a non-zero error code if the process fails to start or terminates prematurely.
 *
 * @param blight Pointer to the QProcess used to launch and monitor the "blight" process.
 * @param app Pointer to the QCoreApplication instance used for processing events while waiting.
 *
 * @return int 0 on success, or a non-zero error code if the service fails to start.
 */
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

/**
 * @brief Executes the C tests after ensuring that the "blight" service is running.
 *
 * This function instantiates a QProcess for the "blight" service and checks its availability
 * via wait_for_service(). If the service fails to start, an error code is returned immediately.
 * Otherwise, it proceeds to run the C tests by invoking test_c(). After test execution, if the
 * "blight" process is still active, it is terminated to ensure proper cleanup.
 *
 * @return int The test result code from test_c(), or an error code from wait_for_service() if the service could not be started.
 */
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

/**
 * @brief Entry point for the application.
 *
 * Initializes the Qt core application, sets the thread name and DPI attribute, and schedules test execution.
 * Depending on the presence of the "SKIP_C_TESTS" environment variable, it conditionally runs C tests before
 * executing additional automated tests. The aggregated test results determine the application's exit code.
 *
 * @param argc The number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return int The final exit status of the application.
 */
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
