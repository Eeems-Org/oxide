#include "test_Debug.h"

#include <liboxide/debug.h>

test_Debug::test_Debug(){ }
test_Debug::~test_Debug(){ }

void test_Debug::test_backtrace(){
    auto trace = Oxide::backtrace(2);
    QCOMPARE(trace.size(), 1);
    QVERIFY(trace[0].starts_with("/usr/share/tests/liboxide/liboxide()"));
}

DECLARE_TEST(test_Debug)
