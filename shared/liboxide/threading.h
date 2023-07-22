/*!
 * \addtogroup Oxide
 * \brief The main Oxide module
 * @{
 * \file
 */
#pragma once
#include <QTimer>

namespace Oxide {
    /*!
     * \brief startThreadWithPriority
     * \param thread
     * \param priority
     */
    LIBOXIDE_EXPORT void startThreadWithPriority(QThread* thread, QThread::Priority priority);
    /*!
     * \brief Run code on the main Qt thread
     * \param callback The code to run on the main thread
     *
     * \snippet examples/oxide.cpp dispatchToMainThread
     */
    LIBOXIDE_EXPORT void dispatchToMainThread(std::function<void()> callback);
    /*!
     * \brief Run code on the main Qt thread
     * \param callback The code to run on the main thread
     * \return Return value of callback
     *
     * \snippet examples/oxide.cpp dispatchToMainThread
     */
    template<typename T> LIBOXIDE_EXPORT T dispatchToMainThread(std::function<T()> callback){ return dispatchToThread<T>(qApp->thread(), callback); }
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
    template<typename T> LIBOXIDE_EXPORT T dispatchToThread(QThread* thread, std::function<T()> callback){
        if(QThread::currentThread() == thread){
            // Already on the correct thread
            return callback();
        }
        // We are on a different thread
        QTimer* timer = new QTimer();
        timer->moveToThread(thread);
        timer->setSingleShot(true);
        T result;
        QObject::connect(timer, &QTimer::timeout, [timer, &result, callback](){
            // This runs on the correct thread
            result = callback();
            timer->deleteLater();
        });
        QMetaObject::invokeMethod(timer, "start", Qt::BlockingQueuedConnection, Q_ARG(int, 0));
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
     * \brief runInEventLoop
     * \param callback
     */
    LIBOXIDE_EXPORT void runInEventLoop(std::function<void(std::function<void()>)> callback);
}
