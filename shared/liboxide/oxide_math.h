/*!
 * \addtogroup Math
 * \brief Math module
 * @{
 * \file math.h
 */
#pragma once

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
    int convertRange(int value, int oldMin, int oldMax, int newMin, int newMax);
    /*!
     * \brief normalize
     * \param value
     * \param min
     * \param max
     * \return
     */
    double normalize(int value, int min, int max);
}
