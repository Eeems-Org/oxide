#pragma once
#include "libblight_global.h"
#include <chrono>

namespace Blight{
    class LIBBLIGHT_EXPORT ClockWatch {
    public:
        std::chrono::high_resolution_clock::time_point t1;

        ClockWatch(){ t1 = std::chrono::high_resolution_clock::now(); }

        std::chrono::duration<double> diff(){
            auto t2 = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
        }

        double elapsed(){ return diff().count(); }
    };
}
