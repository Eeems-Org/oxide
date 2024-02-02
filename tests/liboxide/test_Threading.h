#pragma once
#include "autotest.h"

class test_Threading : public QObject{
    Q_OBJECT

public:
    test_Threading();
    ~test_Threading();

private slots:
    void test_startThreadWithPriority();
    void test_dispatchToThread();
    void test_runLater();
    void test_runInEventLoop();

private:
    static int getNice();
};
