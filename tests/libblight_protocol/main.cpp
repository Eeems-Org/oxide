#include <libblight.h>
#include <libblight/dbus.h>

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QTimer>

#include <cstring>
#include <sys/mman.h>

#include "autotest.h"
#include "test.h"

extern "C" BlightProtocol::blight_input_buffer_t*
create_test_input_buffer() {
  size_t ringSize = sizeof(BlightProtocol::EvdevRingBuffer);
  void* mem = mmap(
    NULL, ringSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
  );
  if (mem == MAP_FAILED) {
    return NULL;
  }
  auto rb = new (mem) BlightProtocol::EvdevRingBuffer(false);
  struct input_event ev;
  std::memset(&ev, 0, sizeof(ev));
  ev.type = EV_KEY;
  ev.code = 42;
  ev.value = 1;
  rb->insert(ev);
  auto buf = new BlightProtocol::blight_input_buffer_t();
  buf->device = 0;
  buf->fd = -1;
  buf->ringBuffer = rb;
  return buf;
}

int
wait_for_service(QProcess* blight, QCoreApplication* app) {
  qDebug() << "Checking to see if blight is running...";
  std::unique_ptr<Blight::DBus> dbus = std::make_unique<Blight::DBus>(
#ifdef EPAPER
    true
#else
    false
#endif
  );
  if (!dbus->has_service(BLIGHT_SERVICE)) {
    blight->start("blight", QStringList());
    qDebug() << "Waiting for blight to start...";
    if (!blight->waitForStarted()) {
      qDebug() << "Failed to start: " << blight->exitCode();
      return 1;
    }
    qDebug() << "Waiting for blight dbus service...";
    while (!dbus->has_service(BLIGHT_SERVICE)) {
      app->processEvents(QEventLoop::AllEvents, 1000);
      if (blight->state() == QProcess::NotRunning) {
        qDebug() << "Blight failed to start:" << blight->exitCode();
        return blight->exitCode() || 1;
      }
    }
  }
  return 0;
}

int
run_c_tests(QCoreApplication* app) {
  QProcess blight;
  int res = wait_for_service(&blight, app);
  if (res) {
    return res;
  }
  res = test_c();
  if (blight.state() != QProcess::NotRunning) {
    qDebug() << "Waiting for blight to stop...";
    blight.kill();
    blight.waitForFinished();
  }
  return res;
}

int
main(int argc, char* argv[]) {
  QThread::currentThread()->setObjectName("main");
  QCoreApplication app(argc, argv);
  app.setAttribute(Qt::AA_Use96Dpi, true);
  QTimer::singleShot(0, [&app, &argc, &argv] {
    int res = 0;
    if (getenv("SKIP_C_TESTS") == nullptr) {
      res = run_c_tests(&app);
    }
    res += AutoTest::run(argc, argv);
    app.exit(res);
  });
  return app.exec();
}
