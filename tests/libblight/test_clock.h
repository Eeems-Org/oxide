#pragma once
#include "autotest.h"

class test_Clock : public QObject{
    Q_OBJECT

public:
   test_Clock();
    ~test_Clock();

private slots:
    void test_diff();
    void test_elapsed();
};
