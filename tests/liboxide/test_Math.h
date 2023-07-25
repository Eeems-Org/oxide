#pragma once
#include "autotest.h"

class test_Math : public QObject{
    Q_OBJECT

public:
    test_Math();
    ~test_Math();

private slots:
    void test_normalize();
    void test_convertRange();
};
