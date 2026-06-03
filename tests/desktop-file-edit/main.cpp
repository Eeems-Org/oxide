#include <QCoreApplication>
#include <QThread>
#include <QTimer>

#include "autotest.h"

int
main(int argc, char* argv[]) {
  QThread::currentThread()->setObjectName("main");
  QCoreApplication app(argc, argv);
  app.setAttribute(Qt::AA_Use96Dpi, true);
  QTimer::singleShot(0, [&app, argc, argv] {
    app.exit(AutoTest::run(argc, argv));
  });
  return app.exec();
}