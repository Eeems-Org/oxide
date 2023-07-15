#include <QtTest>
#include <QImage>
#include <QLocalServer>

#include <liboxide/qt.h>
#include <sys/socket.h>
#include <liboxide/tarnish.h>

using namespace Oxide::Tarnish;

class test_QByteArray : public QObject{
    Q_OBJECT

public:
    test_QByteArray();
    ~test_QByteArray();

private slots:
    void test_qint8();
    void test_qint16();
    void test_qint32();
    void test_qint64();
    void test_quint8();
    void test_quint16();
    void test_quint32();
    void test_quint64();
    void test_RepaintEventArgs();
    void test_GeometryEventArgs();
    void test_ImageInfoEventArgs();
    void test_WaitForPaintEventArgs();
    void test_KeyEventArgs();
    void test_TouchEventArgs();
    void test_WindowEvent_Invalid();
    void test_WindowEvent_Ping();
    void test_WindowEvent_Repaint();
    void test_WindowEvent_WaitForPaint();
    void test_WindowEvent_Geometry();
    void test_WindowEvent_ImageInfo();
    void test_WindowEvent_Raise();
    void test_WindowEvent_Lower();
    void test_WindowEvent_Close();
    void test_WindowEvent_FrameBuffer();
    void test_WindowEvent_Key();
    void test_WindowEvent_Touch();

private:
    QLocalSocket socketIn;
    QLocalSocket socketOut;
};

test_QByteArray::test_QByteArray(){
    int fds[2];
    Q_ASSERT(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != -1);
    Q_ASSERT(socketIn.setSocketDescriptor(fds[0], QLocalSocket::ConnectedState, QLocalSocket::WriteOnly | QLocalSocket::Unbuffered));
    Q_ASSERT(socketOut.setSocketDescriptor(fds[1], QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered));
}

test_QByteArray::~test_QByteArray(){
    socketIn.close();
    socketOut.close();
}

void test_QByteArray::test_qint8(){
    qint8 a = (qint8)1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(qint8));
    qint8 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_qint16(){
    qint16 a = 1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(qint16));
    qint16 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_qint32(){
    qint32 a = 1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(qint32));
    qint32 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_qint64(){
    qint64 a = 1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(qint64));
    qint64 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_quint8(){
    quint8 a = (quint8)1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(quint8));
    quint8 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_quint16(){
    quint16 a = 1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(quint16));
    quint16 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_quint32(){
    quint32 a = 1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(quint32));
    quint32 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_quint64(){
    quint64 a = 1000;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), sizeof(quint64));
    quint64 b;
    d >> b;
    QCOMPARE(a, b);
}

void test_QByteArray::test_RepaintEventArgs(){
    RepaintEventArgs a;
    a.x = -10;
    a.y = -10;
    a.width = 10;
    a.height = 10;
    a.marker = 100;
    a.waveform = EPFrameBuffer::HighQualityGrayscale;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), RepaintEventArgs::size());
    RepaintEventArgs b;
    d >> b;
    QCOMPARE(a.geometry(), b.geometry());
    QCOMPARE((int)a.waveform, (int)b.waveform);
    QCOMPARE(a.marker, b.marker);
}

void test_QByteArray::test_GeometryEventArgs(){
    GeometryEventArgs a;
    a.x = -10;
    a.y = -10;
    a.width = 10;
    a.height = 10;
    a.z = 100;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), GeometryEventArgs::size());
    GeometryEventArgs b;
    d >> b;
    QCOMPARE(a.geometry(), b.geometry());
    QCOMPARE(a.z, b.z);
}

void test_QByteArray::test_ImageInfoEventArgs(){
    ImageInfoEventArgs a;
    a.sizeInBytes = 1000000;
    a.bytesPerLine = 10000;
    a.format = QImage::Format_A2BGR30_Premultiplied;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), ImageInfoEventArgs::size());
    ImageInfoEventArgs b;
    d >> b;
    QCOMPARE(a.sizeInBytes, b.sizeInBytes);
    QCOMPARE(a.bytesPerLine, b.bytesPerLine);
    QCOMPARE(a.format, b.format);
}

void test_QByteArray::test_WaitForPaintEventArgs(){
    WaitForPaintEventArgs a;
    a.marker = 100;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), WaitForPaintEventArgs::size());
    WaitForPaintEventArgs b;
    d >> b;
    QCOMPARE(a.marker, b.marker);
}

void test_QByteArray::test_KeyEventArgs(){
    KeyEventArgs a;
    a.code = 100;
    a.type = KeyEventType::Repeat;
    a.unicode = 'a';
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), KeyEventArgs::size());
    KeyEventArgs b;
    d >> b;
    QCOMPARE(a.code, b.code);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_TouchEventArgs(){
    TouchEventArgs a;
    a.type = TouchEventType::End;
    a.toolType = TouchEventTool::Palm;
    a.pressure = 100;
    a.distance = 100;
    a.orientation = -100;
    a.position.x = -100;
    a.position.y = -100;
    a.position.width = 100;
    a.position.height = 100;
    a.toolPosition.x = -100;
    a.toolPosition.y = -100;
    a.toolPosition.width = 100;
    a.toolPosition.height = 100;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), TouchEventArgs::size());
    TouchEventArgs b;
    d >> b;
    QCOMPARE(a.type, b.type);
    QCOMPARE(a.toolType, b.toolType);
    QCOMPARE(a.pressure, b.pressure);
    QCOMPARE(a.distance, b.distance);
    QCOMPARE(a.orientation, b.orientation);
    QCOMPARE(a.position.geometry(), b.position.geometry());
    QCOMPARE(a.toolPosition.geometry(), b.toolPosition.geometry());
}

void test_QByteArray::test_WindowEvent_Invalid(){
    WindowEvent a;
    a.type = WindowEventType::Invalid;
    a.toSocket(&socketIn);
    // Use socketIn.flush() instead of socketOut.waitForReadyRead()
    // to avoid blocking for 30s
    socketIn.flush();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_WindowEvent_Ping(){
    WindowEvent a;
    a.type = WindowEventType::Ping;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_WindowEvent_Repaint(){
    WindowEvent a;
    a.type = WindowEventType::Repaint;
    a.repaintData.x = -100;
    a.repaintData.y = -100;
    a.repaintData.width = 100;
    a.repaintData.height = 100;
    a.repaintData.waveform = EPFrameBuffer::HighQualityGrayscale;
    a.repaintData.marker = 100;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
    QCOMPARE(a.repaintData.geometry(), b.repaintData.geometry());
    QCOMPARE(a.repaintData.waveform, b.repaintData.waveform);
    QCOMPARE(a.repaintData.marker, b.repaintData.marker);
}

void test_QByteArray::test_WindowEvent_WaitForPaint(){
    WindowEvent a;
    a.type = WindowEventType::WaitForPaint;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_WindowEvent_Geometry(){
    WindowEvent a;
    a.type = WindowEventType::Geometry;
    a.geometryData.x = -100;
    a.geometryData.y = -100;
    a.geometryData.width = 100;
    a.geometryData.height = 100;
    a.geometryData.z = 100;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
    QCOMPARE(a.geometryData.geometry(), b.geometryData.geometry());
    QCOMPARE(a.geometryData.z, b.geometryData.z);
}

void test_QByteArray::test_WindowEvent_ImageInfo(){
    WindowEvent a;
    a.type = WindowEventType::ImageInfo;
    a.imageInfoData.bytesPerLine = 100;
    a.imageInfoData.sizeInBytes = 100;
    a.imageInfoData.format = QImage::Format_Alpha8;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
    QCOMPARE(a.imageInfoData.bytesPerLine, b.imageInfoData.bytesPerLine);
    QCOMPARE(a.imageInfoData.sizeInBytes, b.imageInfoData.sizeInBytes);
    QCOMPARE(a.imageInfoData.format, b.imageInfoData.format);
}

void test_QByteArray::test_WindowEvent_Raise(){
    WindowEvent a;
    a.type = WindowEventType::Raise;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}


void test_QByteArray::test_WindowEvent_Lower(){
    WindowEvent a;
    a.type = WindowEventType::Lower;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_WindowEvent_Close(){
    WindowEvent a;
    a.type = WindowEventType::Close;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_WindowEvent_FrameBuffer(){
    WindowEvent a;
    a.type = WindowEventType::FrameBuffer;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
}

void test_QByteArray::test_WindowEvent_Key(){
    WindowEvent a;
    a.type = WindowEventType::Key;
    a.keyData.code = 100;
    a.keyData.type = KeyEventType::Repeat;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
    QCOMPARE(a.keyData.code, b.keyData.code);
    QCOMPARE(a.keyData.type, b.keyData.type);
}

void test_QByteArray::test_WindowEvent_Touch(){
    WindowEvent a;
    a.type = WindowEventType::Touch;
    a.touchData.type = TouchEventType::End;
    a.touchData.toolType = TouchEventTool::Palm;
    a.touchData.pressure = 100;
    a.touchData.distance = 100;
    a.touchData.orientation = -100;
    a.touchData.id = 100;
    a.touchData.position.x = -100;
    a.touchData.position.y = -100;
    a.touchData.position.width = 100;
    a.touchData.position.height = 100;
    a.touchData.toolPosition.x = -100;
    a.touchData.toolPosition.y = -100;
    a.touchData.toolPosition.width = 100;
    a.touchData.toolPosition.height = 100;
    a.toSocket(&socketIn);
    socketOut.waitForReadyRead();
    auto b = WindowEvent::fromSocket(&socketOut);
    QCOMPARE(a.type, b.type);
    QCOMPARE(a.touchData.type, b.touchData.type);
    QCOMPARE(a.touchData.toolType, b.touchData.toolType);
    QCOMPARE(a.touchData.pressure, b.touchData.pressure);
    QCOMPARE(a.touchData.distance, b.touchData.distance);
    QCOMPARE(a.touchData.orientation, b.touchData.orientation);
    QCOMPARE(a.touchData.id, b.touchData.id);
    QCOMPARE(a.touchData.position.geometry(), b.touchData.position.geometry());
    QCOMPARE(a.touchData.toolPosition.geometry(), b.touchData.toolPosition.geometry());
}

QTEST_APPLESS_MAIN(test_QByteArray)

#include "test_QByteArray.moc"
