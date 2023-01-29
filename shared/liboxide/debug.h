/*!
 * \file debug.h
 */
#pragma once

#include "liboxide_global.h"

#include <QDebug>

/*!
 * \def O_DEBUG(msg)
 * \brief Log a debug message if compiled with DEBUG mode, and debugging is enabled
 * \param Debug message to log
 */
#ifdef DEBUG
#define O_DEBUG(msg) if(Oxide::debugEnabled()){ qDebug() << msg; }
#else
#define O_DEBUG(msg)
#endif
/*!
 * \def O_WARNING(msg)
 * \brief Log a warning message if debugging is enabled
 * \param Warning message to log
 */
#define O_WARNING(msg) if(Oxide::debugEnabled()){ qWarning() << msg; }

namespace Oxide {
    /*!
     * \brief Return the state of debugging
     * \return Debugging state
     * \snippet examples/oxide.cpp debugEnabled
     */
    LIBOXIDE_EXPORT bool debugEnabled();
}
