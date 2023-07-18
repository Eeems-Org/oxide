/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"
#include <unistd.h>

#include <QDebug>
#define __DEBUG_LOCATION__ QString("(%1:%2, %3)").arg(__FILE__).arg(__LINE__).arg(__PRETTY_FUNCTION__).toStdString().c_str()
#define __DEBUG_APPLICATION_INFO__ QString("[%1 %2 %3]").arg(::getpid()).arg(::getpgrp()).arg(Oxide::getAppName().c_str()).toStdString().c_str()
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
#define O_DEBUG(msg) if(Oxide::debugEnabled()){ qDebug() << __DEBUG_APPLICATION_INFO__ << "Debug:" << msg << __DEBUG_LOCATION__; }
#else
#define O_DEBUG(msg)
#endif
/*!
 * \def O_WARNING(msg)
 * \brief Log a warning message if debugging is enabled
 * \param msg Warning message to log
 */
#define O_WARNING(msg) if(Oxide::debugEnabled()){ qWarning() << __DEBUG_APPLICATION_INFO__ << "Warning:" << msg << __DEBUG_LOCATION__; }

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
#define O_EVENT(event) O_DEBUG(__DEBUG_APPLICATION_INFO__ << event.input_event_sec << event.input_event_usec << event.type << event.code << event.value << __DEBUG_LOCATION__);
#else
#define O_EVENT(event)
#endif
/*!
 * \def O_INFO(msg)
 * \brief Log an informational message
 * \param msg Informational message to log
 */
#define O_INFO(msg) qInfo() << __DEBUG_APPLICATION_INFO__ << "Info:" << msg << __DEBUG_LOCATION__;

namespace Oxide {
    /*!
     * \brief Return the state of debugging
     * \return Debugging state
     * \snippet examples/oxide.cpp debugEnabled
     */
    LIBOXIDE_EXPORT bool debugEnabled();
    /*!
     * \brief Get the name of the application
     * \return The name of the application
     */
    LIBOXIDE_EXPORT std::string getAppName();
}
/*! @} */
