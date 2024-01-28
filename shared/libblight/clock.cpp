#include "clock.h"

Blight::ClockWatch::ClockWatch(){ t1 = std::chrono::high_resolution_clock::now(); }

std::chrono::duration<double> Blight::ClockWatch::diff(){
    auto t2 = std::chrono::high_resolution_clock::now();
    if(t2 <= t1){
        return std::chrono::duration<double>(0);
    }
    auto diff = t2 - t1;
    return std::chrono::duration_cast<std::chrono::duration<double>>(diff);
}

double Blight::ClockWatch::elapsed(){ return diff().count(); }
