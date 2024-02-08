#include "test_Threading.h"
#include <liboxide/threading.h>
#include <unistd.h>
#include <sys/resource.h>

using namespace Oxide;

test_Threading::test_Threading(){}
test_Threading::~test_Threading(){}

void test_Threading::test_startThreadWithPriority(){
#ifndef QT_HAS_THREAD_PRIORITY_SCHEDULING
    auto thread = new QThread();
    thread->setObjectName("TimeCriticalPriority");
    startThreadWithPriority(thread, QThread::TimeCriticalPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), -20); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("HighestPriority");
    startThreadWithPriority(thread, QThread::HighestPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), -10); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("HighPriority");
    startThreadWithPriority(thread, QThread::HighPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), -5); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("NormalPriority");
    startThreadWithPriority(thread, QThread::NormalPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), 0); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("InheritPriority");
    startThreadWithPriority(thread, QThread::InheritPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), 0); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("LowPriority");
    startThreadWithPriority(thread, QThread::LowPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), 5); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("LowestPriority");
    startThreadWithPriority(thread, QThread::LowestPriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), 10); });
    thread->quit();
    thread->wait();
    thread->deleteLater();

    thread = new QThread();
    thread->setObjectName("IdlePriority");
    startThreadWithPriority(thread, QThread::IdlePriority);
    thread->moveToThread(thread);
    dispatchToThread(thread, [this]{ QCOMPARE(getNice(), 19); });
    thread->quit();
    thread->wait();
    thread->deleteLater();
#endif
}

void test_Threading::test_dispatchToThread(){
    auto thread = new QThread();
    thread->setObjectName("dispatchToThread");
    thread->start();
    thread->moveToThread(thread);
    QVERIFY(thread->isRunning());
    QVERIFY(QThread::currentThread() != thread);

    dispatchToThread(thread, [thread]{
        QCOMPARE(QThread::currentThread(), thread);
        dispatchToMainThread([thread]{
            QVERIFY(QThread::currentThread() != thread);
            QCOMPARE(QThread::currentThread(), qApp->thread());
        });
        QVERIFY(dispatchToMainThread<bool>([]{ return true; }));
        QVERIFY(!dispatchToMainThread<bool>([]{ return false; }));
        QCOMPARE(dispatchToMainThread<const char*>([]{ return "Hello World!"; }), "Hello World!");
        QCOMPARE(dispatchToMainThread<std::string>([]{ return "Hello World!"; }), "Hello World!");
        QCOMPARE(dispatchToMainThread<QString>([]{ return "Hello World!"; }), QString("Hello World!"));
    });
    QVERIFY(dispatchToThread<bool>(thread, []{ return true; }));
    QVERIFY(!dispatchToThread<bool>(thread, []{ return false; }));
    QCOMPARE(dispatchToThread<const char*>(thread, []{ return "Hello World!"; }), "Hello World!");
    QCOMPARE(dispatchToThread<std::string>(thread, []{ return "Hello World!"; }), "Hello World!");
    QCOMPARE(dispatchToThread<QString>(thread, []{ return "Hello World!"; }), QString("Hello World!"));

    thread->quit();
    thread->wait();
    QVERIFY(thread->isFinished());
    thread->deleteLater();
}

void test_Threading::test_runLater(){
    auto thread = new QThread();
    thread->setObjectName("runLater");
    thread->start();
    thread->moveToThread(thread);
    QVERIFY(thread->isRunning());
    QVERIFY(QThread::currentThread() != thread);

    bool ok = false;
    QEventLoop loop;
    runLater(thread, [&ok, &loop, thread]{
        QCOMPARE(QThread::currentThread(), thread);
        ok = true;
        runLaterInMainThread([ok]{
            QCOMPARE(QThread::currentThread(), qApp->thread());
            QVERIFY(ok);
        });
        loop.quit();
    });
    QVERIFY(!ok);
    loop.exec();
    QVERIFY(ok);

    thread->quit();
    thread->wait();
    QVERIFY(thread->isFinished());
    thread->deleteLater();
}

void test_Threading::test_runInEventLoop(){
    bool ok = false;
    runInEventLoop([&ok](std::function<void()> quit){
        QTimer::singleShot(0, [&ok, quit]{
            ok = true;
            quit();
        });
        QVERIFY(!ok);
    });
    QVERIFY(ok);
}

int test_Threading::getNice(){ return getpriority(PRIO_PROCESS, gettid()); }

DECLARE_TEST(test_Threading)
