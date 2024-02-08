#pragma once

#pragma once
#include "autotest.h"

class test_Event_Device : public QObject{
    Q_OBJECT

public:
    test_Event_Device();
    ~test_Event_Device();

private slots:
    void test_create_device();
    void test_event_device();
};
