#include "test_proc_stat.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Standalone replica of the parsing logic from Connection::isStopped()
// (applications/display-server/connection.cpp).
//
// The /proc/<pid>/stat file has the layout:
//   pid (comm) state ...
// where `comm` is the executable name and may contain spaces and even ')'.
// The fix uses lastIndexOf(')') so that even such names are handled correctly.
// ---------------------------------------------------------------------------
static char
parseProcStatState(const QByteArray& data) {
  int idx = data.lastIndexOf(')');
  if (idx < 0 || idx + 2 >= data.size()) {
    return '\0'; // invalid / not found
  }
  return data.at(idx + 2);
}

// ---------------------------------------------------------------------------
// Convenience: build a synthetic /proc/pid/stat line
// ---------------------------------------------------------------------------
static QByteArray
makeStat(const QString& comm, char state) {
  // Format: "1 (comm) S 0 1 ..."
  return QString("1 (%1) %2 0 1 2 3").arg(comm).arg(state).toUtf8();
}

// ---------------------------------------------------------------------------

test_ProcStat::test_ProcStat() {}
test_ProcStat::~test_ProcStat() {}

void
test_ProcStat::test_parse_simpleProcessName() {
  // Standard case: no spaces or special characters in the process name.
  QByteArray data = makeStat("bash", 'S');
  QCOMPARE(parseProcStatState(data), 'S');
}

void
test_ProcStat::test_parse_processNameWithSpaces() {
  // Process names with embedded spaces were the original bug trigger.
  // The old split-on-space approach would have chosen the wrong field.
  QByteArray data = makeStat("my cool app", 'R');
  QCOMPARE(parseProcStatState(data), 'R');
}

void
test_ProcStat::test_parse_processNameWithParentheses() {
  // Process names can also contain ')'.  lastIndexOf ensures we anchor to the
  // real end of the comm field.
  QByteArray data = makeStat("app(1)", 'S');
  QCOMPARE(parseProcStatState(data), 'S');
}

void
test_ProcStat::test_parse_emptyData_returnsInvalid() {
  QCOMPARE(parseProcStatState(QByteArray()), '\0');
}

void
test_ProcStat::test_parse_noClosingParen_returnsInvalid() {
  // Data without any ')' should return the sentinel '\0'.
  QCOMPARE(parseProcStatState(QByteArray("1 (bash S 0")), '\0');
}

void
test_ProcStat::test_parse_closingParenAtEnd_returnsInvalid() {
  // ')' is the very last character — idx + 2 would be out of bounds.
  QCOMPARE(parseProcStatState(QByteArray("1 (bash)")), '\0');
}

void
test_ProcStat::test_parse_stoppedState() {
  // 'T' means the process is stopped (traced or job-control stopped).
  QByteArray data = makeStat("sleep", 'T');
  QCOMPARE(parseProcStatState(data), 'T');
  QVERIFY(parseProcStatState(data) == 'T');
}

void
test_ProcStat::test_parse_runningState() {
  QByteArray data = makeStat("process", 'R');
  QCOMPARE(parseProcStatState(data), 'R');
  QVERIFY(parseProcStatState(data) != 'T');
}

void
test_ProcStat::test_parse_sleepingState() {
  QByteArray data = makeStat("process", 'S');
  QCOMPARE(parseProcStatState(data), 'S');
  QVERIFY(parseProcStatState(data) != 'T');
}

void
test_ProcStat::test_selfNotStopped() {
  // Read the actual /proc/self/stat for the running test process and verify
  // that the parsing logic returns a non-'T' state (test is not stopped).
  QFile file(QString("/proc/%1/stat").arg(getpid()));
  QVERIFY2(file.open(QFile::ReadOnly), "Could not open /proc/self/stat");
  QByteArray data = file.readAll();
  file.close();

  char state = parseProcStatState(data);
  QVERIFY2(state != '\0', "Could not parse state from /proc/self/stat");
  QVERIFY2(
    state != 'T',
    qPrintable(
      QString("Test process unexpectedly in stopped state: %1").arg(state)
    )
  );
  // State should be one of the expected running states
  QVERIFY(state == 'R' || state == 'S' || state == 'D' || state == 'I');
}

DECLARE_TEST(test_ProcStat)