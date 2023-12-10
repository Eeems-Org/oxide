/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"
#include <unistd.h>

#include <QDebug>

/*!
 * \def DEBUG
 * \brief If this is defined, debug code will be compiled in
 * \note If QT_NO_DEBUG is not defined, DEBUG will be automatically defined
 */
#ifndef DEBUG
#ifndef QT_NO_DEBUG
#define DEBUG
#endif
#endif
/*!
 * \def __RIGHT_HERE__
 * \brief Log the current file and line number
 * \note This is only available if DEBUG is defined
 */
#ifdef DEBUG
#define __RIGHT_HERE__ qDebug() << "<============================" << __FILE__ << ":" << __LINE__;
#else
#define __RIGHT_HERE__
#endif
/*!
 * \def __DEBUG_LOCATION__
 * \brief Get the current debug location
 * \note this is automatically included in O_DEBUG, O_WARNING, O_INFO, and O_EVENT
 */
#define __DEBUG_LOCATION__ Oxide::getDebugLocation(__FILE__, __LINE__, __PRETTY_FUNCTION__).c_str()
/*!
 * \def __DEBUG_APPLICATION_INFO__
 * \brief Get the current application information string
 * \note this is automatically included in O_DEBUG, O_WARNING, O_INFO, and O_EVENT
 */
#define __DEBUG_APPLICATION_INFO__ Oxide::getDebugApplicationInfo().c_str()
/*!
 * \def O_DEBUG(msg)
 * \brief Log a debug message if compiled with DEBUG mode, and debugging is enabled
 * \param msg Debug message to log
 */
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
     * \brief Get a formatted application information string
     * \note this is automatically included in O_DEBUG, O_WARNING, O_INFO, and O_EVENT
     * \return Formatted string containing information about the application and current thread
     */
    LIBOXIDE_EXPORT std::string getDebugApplicationInfo();
    /*!
     * \brief Get a formatted debug information string
     * \note this is automatically included in O_DEBUG, O_WARNING, O_INFO, and O_EVENT
     * \param Name of file
     * \param Line number in file
     * \param Function information
     * \return Formatted debug location string
     */
    LIBOXIDE_EXPORT std::string getDebugLocation(const char* file, unsigned int line, const char* function);
    /*!
     * \brief Return the state of debugging
     * \return Debugging state
     * \snippet examples/oxide.cpp debugEnabled
     */
    LIBOXIDE_EXPORT bool debugEnabled();
    /*!
     * \brief Get the name of the application
     * \param Don't use qApp's application name
     * \return The name of the application
     */
    LIBOXIDE_EXPORT std::string getAppName(bool ignoreQApp);
    /*!
     * \brief Print the current backtrace
     */
    LIBOXIDE_EXPORT std::vector<std::string> backtrace(unsigned short depth);
}
/*! @} */
