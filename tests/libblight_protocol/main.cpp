#include "autotest.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include "test.h"

int main(int argc, char* argv[]){
    QThread::currentThread()->setObjectName("main");
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTimer::singleShot(0, [&app, argc, argv]{
        test_c();
        app.exit(AutoTest::run(argc, argv));
    });
    return app.exec();
}
