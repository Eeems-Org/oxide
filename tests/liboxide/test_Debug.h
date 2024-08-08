#pragma once

#pragma once
#include "autotest.h"

class test_Debug : public QObject{
    Q_OBJECT

public:
    test_Debug();
    ~test_Debug();

private slots:
    void test_getDebugApplicationInfo();
    void test_getDebugLocation();
    void test_debugEnabled();
    void test_getAppName();
    void test_backtrace();
};
