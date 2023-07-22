#include "threading.h"
#include "debug.h"

namespace Oxide{
    void startThreadWithPriority(QThread* thread, QThread::Priority priority){
        O_DEBUG(thread << "Starting thread");
#ifndef QT_HAS_THREAD_PRIORITY_SCHEDULING
        QObject::connect(thread, &QThread::started, thread, [thread, priority]{
            switch(priority){
                case QThread::IdlePriority:
                    nice(20);
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

    void dispatchToThread(QThread* thread, std::function<void()> callback){
        dispatchToThread<int>(thread, [callback]{
            callback();
            return 0;
        });
    }

    void dispatchToMainThread(std::function<void()> callback){ dispatchToThread(qApp->thread(), [callback]{ callback(); }); }

    void runLater(QThread* thread, std::function<void ()> callback){
        QTimer* timer = new QTimer();
        timer->moveToThread(thread);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, [timer, callback]{
            callback();
            timer->deleteLater();
        });
        QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
    }

    void runLaterInMainThread(std::function<void ()> callback){ runLater(qApp->thread(), callback); }

    void runInEventLoop(std::function<void (std::function<void()>)> callback){
        QEventLoop loop;
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, [&loop, &timer, callback]{
            if(loop.isRunning()){
                timer.stop();
                callback([&loop]{ loop.quit(); });
            }
        });
        timer.start(0);
        loop.exec();
    }
}
