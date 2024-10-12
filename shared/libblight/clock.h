/*!
 * \addtogroup Blight
 * @{
 * \file
 */
#pragma once
#include "libblight_global.h"
#include <chrono>

namespace Blight{
    /*!
     * \brief The ClockWatch class
     *
     * A simple timer class for determening how long code takes.
     */
    class LIBBLIGHT_EXPORT ClockWatch {
    public:
        /*!
         * \brief Create a new instance and start the timer.
         */
        ClockWatch();
        /*!
         * \brief Get the duration since the ClockWatch was created.
         * \return Duration since the ClockWatch was created.
         */
        std::chrono::duration<double> diff();
        /*!
         * \brief Get the elapsed seconds since the ClockWatch was created.
         * \return The elapsed seconds since the ClockWatch was created.
         */
        double elapsed();

    private:
        std::chrono::high_resolution_clock::time_point t1;
    };
}
/*! @} */
