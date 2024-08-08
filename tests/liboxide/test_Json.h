#pragma once

#pragma once
#include "autotest.h"

class test_Json : public QObject{
    Q_OBJECT

public:
    test_Json();
    ~test_Json();

private slots:
    void test_decodeDBusArgument();
    void test_sanitizeForJson();
    void test_toJson();
    void test_fromJson();
};
