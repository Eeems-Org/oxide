#include "test_Ipc.h"

#include <liboxide/debug.h>
#include <liboxide/ipc.h>

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryFile>
#include <QTest>
#include <QThread>
#include <QTimer>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace Oxide;

test_Ipc::test_Ipc() {}
test_Ipc::~test_Ipc() {}

void
test_Ipc::test_singleInstance() {
  IPC primary("single");
  QVERIFY(primary.start());
  IPC secondary("single");
  QVERIFY(!secondary.start());
}

void
test_Ipc::test_sendFiles() {
  IPC primary("files");
  QVERIFY(primary.start());

  QStringList receivedFiles;
  QEventLoop loop;
  QObject::connect(&primary, &IPC::files, [&](const QStringList& files) {
    receivedFiles = files;
    loop.quit();
  });

  QStringList files = {"/path/to/file1.txt", "/path/to/file2.txt"};

  QThread senderThread;
  senderThread.start();
  IPC sender("files");
  sender.moveToThread(&senderThread);
  QMetaObject::invokeMethod(&sender, [&sender, files]() {
    QVERIFY(!sender.start());
    QVERIFY(sender.send(files));
  });

  QTimer::singleShot(2000, &loop, &QEventLoop::quit);
  loop.exec();

  senderThread.quit();
  senderThread.wait();

  QCOMPARE(receivedFiles, files);
}

void
test_Ipc::test_sendMessage() {
  IPC primary("message");
  QVERIFY(primary.start());

  QByteArray receivedMsg;
  QEventLoop loop;
  QObject::connect(&primary, &IPC::message, [&](const QByteArray& data) {
    receivedMsg = data;
    loop.quit();
  });

  QByteArray msgData = "test message data";

  QThread senderThread;
  senderThread.start();
  IPC sender("message");
  sender.moveToThread(&senderThread);
  QMetaObject::invokeMethod(&sender, [&sender, msgData]() {
    QVERIFY(!sender.start());
    QVERIFY(sender.send(msgData));
  });

  QTimer::singleShot(2000, &loop, &QEventLoop::quit);
  loop.exec();

  senderThread.quit();
  senderThread.wait();

  QCOMPARE(receivedMsg, msgData);
}

void
test_Ipc::test_timeout() {
  IPC sender("timeout_sender");
  QStringList files = {"file.txt"};
  QVERIFY(!sender.send(files, 500));
}

DECLARE_TEST(test_Ipc)
