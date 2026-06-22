#include "test_Debug.h"

#include <liboxide/debug.h>

#include <QFileInfo>

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
  auto traceLine = QString::fromStdString(trace[0]);
  auto parenIndex = traceLine.indexOf('(');
  QVERIFY2(
    parenIndex != -1, qPrintable("Backtrace line missing '(' : " + traceLine)
  );
  auto path = QFileInfo(traceLine.left(parenIndex));
  auto expectedPrefix =
    QString(TESTS_INSTALL_PATH) + QStringLiteral("/liboxide");
  QVERIFY2(
    path.exists(),
    qPrintable(
      QStringLiteral("Backtrace file does not exist: %1").arg(path.filePath())
    )
  );
  QVERIFY2(
    path.canonicalFilePath().startsWith(expectedPrefix),
    qPrintable(QStringLiteral(
                 "Backtrace does not start with expected prefix.\n"
                 "  Resolved: %1\n"
                 "  Expected prefix: %2"
    )
                 .arg(path.canonicalFilePath(), expectedPrefix))
  );
}

DECLARE_TEST(test_Debug)
