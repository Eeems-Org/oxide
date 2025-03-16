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
 * @brief Waits for the BLIGHT service to become available.
 *
 * This function checks if the BLIGHT service is registered on D-Bus. If the 
 * service is not available, it attempts to start the blight process located at 
 * /opt/bin/blight using the provided QProcess instance. It processes application 
 * events through the given QCoreApplication pointer until the service is detected. 
 * If the process fails to start or terminates unexpectedly before the service is 
 * available, an error code is returned.
 *
 * @param blight Pointer to the QProcess instance managing the blight process.
 * @param app Pointer to the QCoreApplication instance used for event processing.
 * @return int Returns 0 if the service is available or successfully started; otherwise, an error code.
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
 * @brief Executes C tests after ensuring the "blight" service is available.
 *
 * This function starts by initializing the "blight" process via wait_for_service(). If the
 * service cannot be started or is unavailable, it returns the corresponding error code immediately.
 * Otherwise, it runs the C tests, and if the "blight" process remains active after testing,
 * it is terminated before returning the test result.
 *
 * @return int Error code from wait_for_service() if service initialization fails, or the
 *             result of test_c() when tests complete.
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
 * @brief Entry point for the Qt-based test application.
 *
 * Initializes the Qt application with DPI and thread settings and schedules
 * deferred test execution using a single-shot timer. The deferred routine conditionally
 * runs C tests via `run_c_tests` (unless skipped by the "SKIP_C_TESTS" environment variable)
 * and executes additional tests via `AutoTest::run`. The application exits with the
 * combined test result.
 *
 * @param argc Standard argument count.
 * @param argv Standard argument vector.
 * @return int Application exit status.
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
