/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QDebug>
/*!
 * \def O_DEBUG(msg)
 * \brief Log a debug message if compiled with DEBUG mode, and debugging is enabled
 * \param msg Debug message to log
 */
#ifndef DEBUG
#ifndef QT_NO_DEBUG
#define DEBUG
#endif
#endif
#ifdef DEBUG
#define O_DEBUG(msg) if(Oxide::debugEnabled()){ qDebug() << msg; }
#else
#define O_DEBUG(msg)
#endif
/*!
 * \def O_WARNING(msg)
 * \brief Log a warning message if debugging is enabled
 * \param msg Warning message to log
 */
#define O_WARNING(msg) if(Oxide::debugEnabled()){ qWarning() << msg; }

/*!
 * \def O_EVENT(msg)
 * \brief Debug log an input_event if debugging is enabled
 * \param event input_event to log
 */
#ifdef DEBUG_EVENTS
#ifdef input_event_sec
#define input_event_sec time.tv_sec
#endif
#ifdef input_event_usec
#define input_event_usec time.tv_usec
#endif
#define O_EVENT(event) O_DEBUG(__PRETTY_FUNCTION__ << event.input_event_sec << event.input_event_usec << event.type << event.code << event.value);
#else
#define O_EVENT(event)
#endif
/*!
 * \def O_INFO(msg)
 * \brief Log an informational message
 * \param msg Informational message to log
 */
#define O_INFO(msg) qInfo() << msg;

namespace Oxide {
    /*!
     * \brief Return the state of debugging
     * \return Debugging state
     * \snippet examples/oxide.cpp debugEnabled
     */
    LIBOXIDE_EXPORT bool debugEnabled();
}
/*! @} */
