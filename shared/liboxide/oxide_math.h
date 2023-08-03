/*!
 * \addtogroup Math
 * \brief Math module
 * @{
 * \file math.h
 */
#pragma once
#include "liboxide_global.h"
#include <QPointF>
#include <QRect>

namespace Oxide::Math{
    /*!
     * \brief convertRange
     * \param value
     * \param oldMin
     * \param oldMax
     * \param newMin
     * \param newMax
     * \return
     */
    LIBOXIDE_EXPORT int convertRange(int value, int oldMin, int oldMax, int newMin, int newMax);
    /*!
     * \brief normalize
     * \param value
     * \param min
     * \param max
     * \return
     */
    LIBOXIDE_EXPORT double normalize(int value, int min, int max);
    /*!
     * \brief normalize
     * \param pos
     * \param bounds
     * \return
     */
    LIBOXIDE_EXPORT QPointF normalize(QPoint pos, QRect bounds);
    /*!
     * \brief normalize
     * \param pos
     * \param bounds
     * \return
     */
    LIBOXIDE_EXPORT QRectF normalize(QRect rect, QRect bounds);
}
