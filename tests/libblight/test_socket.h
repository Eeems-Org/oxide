#pragma once
#include "autotest.h"

class test_Socket : public QObject{
    Q_OBJECT

public:
    test_Socket();
    ~test_Socket();

private slots:
    void test_recv();
    void test_recv_blocking();
    void test_send_blocking();
    void test_wait_for_send();
    void test_wait_for_read();
};
