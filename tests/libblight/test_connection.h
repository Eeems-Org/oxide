#pragma once
#include "autotest.h"

class test_Connection : public QObject {
  Q_OBJECT

public:
  test_Connection();
  ~test_Connection();

private slots:
  void test_surfaces_data_parsing();
};
