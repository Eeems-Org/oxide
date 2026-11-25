#pragma once
#include "autotest.h"

class test_Ipc : public QObject {
  Q_OBJECT

public:
  test_Ipc();
  ~test_Ipc();

private slots:
  void test_singleInstance();
  void test_sendFiles();
  void test_sendMessage();
  void test_timeout();
};