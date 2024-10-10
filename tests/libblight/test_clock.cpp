#include "test_clock.h"

#include <libblight/clock.h>

test_Clock::test_Clock(){ }
test_Clock::~test_Clock(){ }

void test_Clock::test_diff(){
    auto cw = Blight::ClockWatch();
    QTRY_VERIFY(cw.diff().count() > 0);
}

void test_Clock::test_elapsed(){
    auto cw = Blight::ClockWatch();
    QTRY_VERIFY(cw.elapsed() > 0);
}

DECLARE_TEST(test_Clock)
