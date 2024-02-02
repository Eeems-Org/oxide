#include "test_Event_Device.h"

#include <liboxide/event_device.h>

test_Event_Device::test_Event_Device(){ }
test_Event_Device::~test_Event_Device(){ }

void test_Event_Device::test_create_device(){
    auto ev = Oxide::event_device::create_event(0, 0, 0);
    QCOMPARE(ev.type, 0);
    QCOMPARE(ev.code, 0);
    QCOMPARE(ev.value, 0);
    ev = Oxide::event_device::create_event(1, 1, 1);
    QCOMPARE(ev.type, 1);
    QCOMPARE(ev.code, 1);
    QCOMPARE(ev.value, 1);
}

void test_Event_Device::test_event_device(){
    Oxide::event_device event0("/dev/input/event0", O_RDWR);
    QCOMPARE(event0.device, "/dev/input/event0");
    QVERIFY(!event0.locked);
    event0.lock();
    QVERIFY(event0.locked);
    event0.unlock();
    QVERIFY(!event0.locked);
    QVERIFY(event0.fd > 0);
    event0.close();
    QCOMPARE(event0.fd, 0);
}

DECLARE_TEST(test_Event_Device)
