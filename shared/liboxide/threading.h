/*!
 * \addtogroup Oxide
 * \brief The main Oxide module
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include <QTimer>
#include <QThread>
#include <QCoreApplication>

namespace Oxide {
    /*!
     * \brief Start a QThread with a specific priority. This will work even if Qt doesn't support priority scheduling
     * \param thread Thread to start
     * \param priority Priority to start with
     */
    LIBOXIDE_EXPORT void startThreadWithPriority(QThread* thread, QThread::Priority priority);
    /*!
     * \brief Run code on the main Qt thread
     * \param callback The code to run on the main thread
     *  * \snippet examples/oxide.cpp dispatchToMainThread
     */
    LIBOXIDE_EXPORT void dispatchToMainThread(std::function<void()> callback);
    /*!
     * \brief Run code on the main Qt thread
     * \param callback The code to run on the main thread
     * \return Return value of callback
     *  * \snippet examples/oxide.cpp dispatchToMainThread
     */
    template<typename T> LIBOXIDE_EXPORT T dispatchToMainThread(std::function<T()> callback){
        return dispatchToThread<T>(qApp->thread(), callback);
    }
    /*!
     * \brief Run code on a specific thread
     * \param thread The thread to run the callback in
     * \param callback The code to run on the thread
     */
    LIBOXIDE_EXPORT void dispatchToThread(QThread* thread, std::function<void()> callback);
    /*!
     * \brief Run code on a specific thread
     * \param thread The thread to run the callback in
     * \param callback The code to run on the thread
     * \return Return value of callback
     */
    template<typename T> LIBOXIDE_EXPORT T dispatchToThread(
        QThread* thread,
        std::function<T()> callback
    ){
        T result;
        dispatchToThread(thread, [callback, &result]{
            result = callback();
        });
        return result;
    }
    /*!
     * \brief Run code on a specific thread at some point in the near future
     * \param thread The thread to run the callback in
     * \param callback The code to run on the thread
     */
    LIBOXIDE_EXPORT void runLater(QThread* thread, std::function<void()> callback);
    /*!
     * \brief Run code on the main thread at some point in the near future
     * \param callback The code to run on the thread
     */
    LIBOXIDE_EXPORT void runLaterInMainThread(std::function<void()> callback);
    /*!
     * \brief Run code in a Qt event loop
     * \param callback The code to run inside an event loop
     */
    LIBOXIDE_EXPORT void runInEventLoop(std::function<void(std::function<void()>)> callback);
}
/*! @} */
