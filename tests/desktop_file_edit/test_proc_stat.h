#pragma once
#include "autotest.h"

// Tests for the /proc/pid/stat parsing logic introduced in
// Connection::isStopped() (applications/display-server/connection.cpp).
//
// The original code split on ' ' and took index 2, which broke for process
// names containing spaces. The new code finds the last ')' and reads the
// character two positions after it (skipping the trailing space).
class test_ProcStat : public QObject {
  Q_OBJECT

public:
  test_ProcStat();
  ~test_ProcStat();

private slots:
  // Verify the helper correctly extracts the state field.
  void test_parse_simpleProcessName();
  void test_parse_processNameWithSpaces();
  void test_parse_processNameWithParentheses();
  void test_parse_emptyData_returnsInvalid();
  void test_parse_noClosingParen_returnsInvalid();
  void test_parse_closingParenAtEnd_returnsInvalid();
  void test_parse_stoppedState();
  void test_parse_runningState();
  void test_parse_sleepingState();

  // Integration: current process must not be stopped.
  void test_selfNotStopped();
};