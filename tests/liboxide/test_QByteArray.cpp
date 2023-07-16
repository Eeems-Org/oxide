#include <QtTest>
#include <QImage>
#include <QBuffer>

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
    void test_TabletEventArgs();
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
    void test_WindowEvent_Tablet();

private:
    QByteArray buffer;
    QBuffer socket;
    void setup_WindowEvent();
};

test_QByteArray::test_QByteArray() : socket{&buffer}{
    QVERIFY(socket.open(QIODevice::ReadWrite));
}

test_QByteArray::~test_QByteArray(){}

void test_QByteArray::setup_WindowEvent(){
    socket.close();
    buffer.clear();
    QVERIFY(socket.atEnd());
    QVERIFY(socket.open(QIODevice::ReadWrite));
    QVERIFY(socket.atEnd());
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
    QCOMPARE((int)a.format, (int)b.format);
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
    a.type = KeyEventType::RepeatKey;
    a.unicode = 'a';
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), KeyEventArgs::size());
    KeyEventArgs b;
    d >> b;
    QCOMPARE(a.code, b.code);
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
}

void test_QByteArray::test_TouchEventArgs(){
    TouchEventArgs a;
    a.type = TouchEventType::TouchRelease;
    a.count = 2;
    TouchEventPoint p;
    p.id = -100;
    p.x = -100;
    p.y = -100;
    p.width = 100;
    p.height = 100;
    p.tool = TouchEventTool::Token;
    p.pressure = 100;
    p.orientation = -100;
    a.points[0] = p;
    p.id = -100;
    p.x = -100;
    p.y = -100;
    p.width = 100;
    p.height = 100;
    p.tool = TouchEventTool::Token;
    p.pressure = 100;
    p.orientation = -100;
    a.points[1] = p;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), a.realSize());
    TouchEventArgs b;
    d >> b;
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE(a.count, b.count);
    for(int i = 0; i < a.count; i++){
        auto a1 = a.points[i];
        auto b1 = b.points[i];
        QCOMPARE(a1.id, b1.id);
        QCOMPARE(a1.x, b1.x);
        QCOMPARE(a1.y, b1.y);
        QCOMPARE(a1.width, b1.width);
        QCOMPARE(a1.height, b1.height);
        QCOMPARE((unsigned short)a1.tool, (unsigned short)b1.tool);
        QCOMPARE(a1.pressure, b1.pressure);
        QCOMPARE(a1.orientation, b1.orientation);
    }
}

void test_QByteArray::test_TabletEventArgs(){
    TabletEventArgs a;
    a.type = TabletEventType::PenRelease;
    a.tool = TabletEventTool::Eraser;
    a.x = -100;
    a.y = -100;
    a.pressure = 100;
    a.tiltX = -100;
    a.tiltY = -100;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), TabletEventArgs::size());
    TabletEventArgs b;
    d >> b;
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE((unsigned short)a.tool, (unsigned short)b.tool);
    QCOMPARE(a.point(), b.point());
    QCOMPARE(a.pressure, b.pressure);
    QCOMPARE(a.tiltX, b.tiltX);
    QCOMPARE(a.tiltY, b.tiltY);
}

void test_QByteArray::test_WindowEvent_Invalid(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Invalid;
    QVERIFY(!a.toSocket(&socket));
    QVERIFY(socket.atEnd());
}

void test_QByteArray::test_WindowEvent_Ping(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Ping;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short));
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
}

void test_QByteArray::test_WindowEvent_Repaint(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Repaint;
    a.repaintData.x = -100;
    a.repaintData.y = -100;
    a.repaintData.width = 100;
    a.repaintData.height = 100;
    a.repaintData.waveform = EPFrameBuffer::HighQualityGrayscale;
    a.repaintData.marker = 100;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + RepaintEventArgs::size());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE(a.repaintData.geometry(), b.repaintData.geometry());
    QCOMPARE(a.repaintData.waveform, b.repaintData.waveform);
    QCOMPARE(a.repaintData.marker, b.repaintData.marker);
}

void test_QByteArray::test_WindowEvent_WaitForPaint(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::WaitForPaint;
    a.waitForPaintData.marker = 100;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + WaitForPaintEventArgs::size());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE(a.waitForPaintData.marker, b.waitForPaintData.marker);
}

void test_QByteArray::test_WindowEvent_Geometry(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Geometry;
    a.geometryData.x = -100;
    a.geometryData.y = -100;
    a.geometryData.width = 100;
    a.geometryData.height = 100;
    a.geometryData.z = 100;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + GeometryEventArgs::size());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE(a.geometryData.geometry(), b.geometryData.geometry());
    QCOMPARE(a.geometryData.z, b.geometryData.z);
}

void test_QByteArray::test_WindowEvent_ImageInfo(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::ImageInfo;
    a.imageInfoData.bytesPerLine = 100;
    a.imageInfoData.sizeInBytes = 100;
    a.imageInfoData.format = QImage::Format_Alpha8;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + ImageInfoEventArgs::size());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE(a.imageInfoData.bytesPerLine, b.imageInfoData.bytesPerLine);
    QCOMPARE(a.imageInfoData.sizeInBytes, b.imageInfoData.sizeInBytes);
    QCOMPARE(a.imageInfoData.format, b.imageInfoData.format);
}

void test_QByteArray::test_WindowEvent_Raise(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Raise;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short));
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
}

void test_QByteArray::test_WindowEvent_Lower(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Lower;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short));
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
}

void test_QByteArray::test_WindowEvent_Close(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Close;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short));
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
}

void test_QByteArray::test_WindowEvent_FrameBuffer(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::FrameBuffer;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short));
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
}

void test_QByteArray::test_WindowEvent_Key(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Key;
    a.keyData.code = 100;
    a.keyData.type = KeyEventType::RepeatKey;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + KeyEventArgs::size());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE(a.keyData.code, b.keyData.code);
    QCOMPARE(a.keyData.type, b.keyData.type);
}

void test_QByteArray::test_WindowEvent_Touch(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Touch;
    a.touchData.type = TouchEventType::TouchRelease;
    a.touchData.count = 2;
    TouchEventPoint p;
    p.id = -100;
    p.x = -100;
    p.y = -100;
    p.width = 100;
    p.height = 100;
    p.tool = TouchEventTool::Token;
    p.pressure = 100;
    p.orientation = -100;
    a.touchData.points[0] = p;
    p.id = -100;
    p.x = -100;
    p.y = -100;
    p.width = 100;
    p.height = 100;
    p.tool = TouchEventTool::Token;
    p.pressure = 100;
    p.orientation = -100;
    a.touchData.points[1] = p;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + a.touchData.realSize());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE((unsigned short)a.touchData.type, (unsigned short)b.touchData.type);
    QCOMPARE(a.touchData.count, b.touchData.count);
    for(int i = 0; i < a.touchData.count; i++){
        auto a1 = a.touchData.points[i];
        auto b1 = b.touchData.points[i];
        QCOMPARE(a1.id, b1.id);
        QCOMPARE(a1.x, b1.x);
        QCOMPARE(a1.y, b1.y);
        QCOMPARE(a1.width, b1.width);
        QCOMPARE(a1.height, b1.height);
        QCOMPARE((unsigned short)a1.tool, (unsigned short)b1.tool);
        QCOMPARE(a1.pressure, b1.pressure);
        QCOMPARE(a1.orientation, b1.orientation);
    }
}

void test_QByteArray::test_WindowEvent_Tablet(){
    setup_WindowEvent();
    WindowEvent a;
    a.type = WindowEventType::Tablet;
    a.tabletData.type = TabletEventType::PenRelease;
    a.tabletData.tool = TabletEventTool::Eraser;
    a.tabletData.x = -100;
    a.tabletData.y = -100;
    a.tabletData.pressure = 100;
    a.tabletData.tiltX = -100;
    a.tabletData.tiltY = -100;
    a.toSocket(&socket);
    QCOMPARE(buffer.size(), sizeof(unsigned short) + TabletEventArgs::size());
    socket.seek(0);
    auto b = WindowEvent::fromSocket(&socket);
    QVERIFY(socket.atEnd());
    QCOMPARE((unsigned short)a.type, (unsigned short)b.type);
    QCOMPARE((unsigned short)a.tabletData.type, (unsigned short)b.tabletData.type);
    QCOMPARE((unsigned short)a.tabletData.tool, (unsigned short)b.tabletData.tool);
    QCOMPARE(a.tabletData.point(), b.tabletData.point());
    QCOMPARE(a.tabletData.pressure, b.tabletData.pressure);
    QCOMPARE(a.tabletData.tiltX, b.tabletData.tiltX);
    QCOMPARE(a.tabletData.tiltY, b.tabletData.tiltY);
}

QTEST_APPLESS_MAIN(test_QByteArray)

#include "test_QByteArray.moc"
