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

private:
    QRect rect(int x0, int y0, int x1, int y1);
    QRectF rectF(double x0, double y0, double x1, double y1);
};
