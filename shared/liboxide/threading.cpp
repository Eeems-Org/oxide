#include "threading.h"
#include "debug.h"

namespace Oxide{
    void startThreadWithPriority(QThread* thread, QThread::Priority priority){
        O_DEBUG(thread << "Starting thread");
    #ifndef QT_HAS_THREAD_PRIORITY_SCHEDULING
        QObject::connect(thread, &QThread::started, thread, [priority]{
            switch(priority){
                case QThread::IdlePriority:
                    nice(19); // 19 is the max
                    break;
                case QThread::LowestPriority:
                    nice(10);
                    break;
                case QThread::LowPriority:
                    nice(5);
                    break;
                case QThread::InheritPriority:
                case QThread::NormalPriority:
                    nice(0);
                    break;
                case QThread::HighPriority:
                    nice(-5);
                    break;
                case QThread::HighestPriority:
                    nice(-10);
                    break;
                case QThread::TimeCriticalPriority:
                    nice(-20);
                    break;
            }
        }, Qt::QueuedConnection);
    #endif
        thread->start(priority);
    }

    void dispatchToMainThread(std::function<void()> callback){
        dispatchToMainThread<int>([callback]{
            callback();
            return 0;
        });
    }

    void dispatchToThread(QThread* thread, std::function<void()> callback){
        if(QThread::currentThread() == thread){
            // Already on the correct thread
            return callback();
        }
        // We are on a different thread
        Q_ASSERT(!QCoreApplication::startingUp());
        O_DEBUG("Dispatching to thread" << thread);
        QTimer* timer = new QTimer();
        timer->setSingleShot(true);
        timer->moveToThread(thread);
        bool finished = false;
        QObject::connect(timer, &QTimer::timeout, thread, [callback, thread, &finished]{
            Q_ASSERT(QThread::currentThread() == thread);
            // This runs on the correct thread
            callback();
            finished = true;
        });
        QMetaObject::invokeMethod(timer, "start", Qt::BlockingQueuedConnection, Q_ARG(int, 0));
        // Don't use an event loop to avoid deadlocks
        while(!finished){
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        timer->deleteLater();
        O_DEBUG("Thread dispatch finished" << thread);
    }

    void runLater(QThread* thread, std::function<void ()> callback){
        O_DEBUG("Run later on thread" << thread);
        QTimer* timer = new QTimer();
        timer->moveToThread(thread);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, thread, [timer, callback, thread]{
            callback();
            timer->deleteLater();
            O_DEBUG("Ran on thread" << thread);
        });
        QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
    }

    void runLaterInMainThread(std::function<void ()> callback){ runLater(qApp->thread(), callback); }

    void runInEventLoop(std::function<void (std::function<void()>)> callback){
        QEventLoop loop;
        QTimer timer;
        O_DEBUG("Running in event loop");
        QObject::connect(&timer, &QTimer::timeout, qApp, [&loop, &timer, callback]{
            if(!loop.isRunning()){
                return;
            }
            timer.stop();
            callback([&loop]{
                loop.quit();
                O_DEBUG("Finished running in event loop");
            });
        });
        timer.start(0);
        loop.exec();
    }
}
