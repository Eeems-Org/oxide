#if defined(LIBOXIDE_LIBRARY)
#include "oxide_math.h"
#else
#include "math.h"
#endif

namespace Oxide::Math{
    int convertRange(int value, int oldMin, int oldMax, int newMin, int newMax){
        Q_ASSERT(oldMin != oldMax);
        Q_ASSERT(newMin != newMax);
        return (normalize(value, oldMin, oldMax) * (newMax - newMin)) + newMin;
    }

    double normalize(int value, int min, int max){
        Q_ASSERT(min != max);
        return (value - min) / double(max - min);
    }

    QPointF normalize(QPoint pos, QRect bounds){
        return QPointF(
            normalize(pos.x(), bounds.left(), bounds.right()),
            normalize(pos.y(), bounds.top(), bounds.bottom())
        );
    }
}
