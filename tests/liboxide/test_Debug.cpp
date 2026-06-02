#include "test_Debug.h"

#include <cstdlib>
#include <liboxide/debug.h>

test_Debug::test_Debug()
  : debug()
  , envWasSet(false) {
  auto env = std::getenv("DEBUG");
  if (env != nullptr) {
    debug = env;
    envWasSet = true;
  }
}
test_Debug::~test_Debug() {
  if (envWasSet) {
    setenv("DEBUG", debug.c_str(), true);
  } else {
    unsetenv("DEBUG");
  }
}

void
test_Debug::test_getDebugApplicationInfo() {
  QCOMPARE(
    QString::fromStdString(Oxide::getDebugApplicationInfo()),
    QString("[%1:%2:%3 liboxide - liboxide]")
      .arg(::getpgrp())
      .arg(::getpid())
      .arg(::gettid())
  );
}

void
test_Debug::test_getDebugLocation() {
  QCOMPARE(
    QString::fromStdString(Oxide::getDebugLocation("file", 0, "function")),
    "(file:0, function)"
  );
  QCOMPARE(
    QString::fromStdString(Oxide::getDebugLocation("file", 1, "function")),
    "(file:1, function)"
  );
}

void
test_Debug::test_debugEnabled() {
  unsetenv("DEBUG");
  QVERIFY(!Oxide::debugEnabled());
  ::setenv("DEBUG", "0", true);
  QVERIFY(!Oxide::debugEnabled());
  ::setenv("DEBUG", "n", true);
  QVERIFY(!Oxide::debugEnabled());
  ::setenv("DEBUG", "no", true);
  QVERIFY(!Oxide::debugEnabled());
  ::setenv("DEBUG", "false", true);
  QVERIFY(!Oxide::debugEnabled());
  ::setenv("DEBUG", "1", true);
  QVERIFY(Oxide::debugEnabled());
  // Cleanup
  if (envWasSet) {
    setenv("DEBUG", debug.c_str(), true);
  } else {
    unsetenv("DEBUG");
  }
}

void
test_Debug::test_getAppName() {
  QCOMPARE(
    QString::fromStdString(Oxide::getAppName(false)), QString("liboxide")
  );
  qApp->setApplicationName("liboxide-test");
  QCOMPARE(
    QString::fromStdString(Oxide::getAppName(false)), QString("liboxide-test")
  );
  qApp->setApplicationName("liboxide");
  QCOMPARE(
    QString::fromStdString(Oxide::getAppName(false)), QString("liboxide")
  );
  QCOMPARE(
    QString::fromStdString(Oxide::getAppName(true)), QString("liboxide")
  );
  // TODO - test /proc/self/comm changing
  // TODO - test /proc/self/exe changing
  // TODO - test /proc/self/{comm,exe} both being null
}

void
test_Debug::test_backtrace() {
  auto trace = Oxide::backtrace(2);
  QCOMPARE(trace.size(), 1);
  QVERIFY(
    QString::fromStdString(trace[0]).startsWith("/opt/share/tests/liboxide(")
  );
}

DECLARE_TEST(test_Debug)
