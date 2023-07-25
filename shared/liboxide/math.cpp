#include "liboxide_global.h"
#include "math.h"

namespace Oxide::Math{
    LIBOXIDE_EXPORT int convertRange(int value, int oldMin, int oldMax, int newMin, int newMax){
        Q_ASSERT(oldMin != oldMax);
        Q_ASSERT(newMin != newMax);
        return (normalize(value, oldMin, oldMax) * (newMax - newMin)) + newMin;
    }

    LIBOXIDE_EXPORT double normalize(int value, int min, int max){
        Q_ASSERT(min != max);
        return (value - min) / qreal(max - min);
    }
}
