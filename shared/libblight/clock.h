#pragma once
#include "libblight_global.h"
#include <chrono>

namespace Blight{
    class LIBBLIGHT_EXPORT ClockWatch {
    public:
        ClockWatch();
        std::chrono::duration<double> diff();
        double elapsed();

    private:
        std::chrono::high_resolution_clock::time_point t1;
    };
}
