#include <QtTest>
#include <QImage>
#include <liboxide/qt.h>
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
};

test_QByteArray::test_QByteArray(){

}

test_QByteArray::~test_QByteArray(){

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
    a.geometry.setRect(10, 10, 10, 10);
    a.marker = 100;
    a.waveform = EPFrameBuffer::HighQualityGrayscale;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), RepaintEventArgs::size);
    RepaintEventArgs b;
    d >> b;
    QCOMPARE(a.geometry, b.geometry);
    QCOMPARE((int)a.waveform, (int)b.waveform);
    QCOMPARE(a.marker, b.marker);
}

void test_QByteArray::test_GeometryEventArgs(){
    GeometryEventArgs a;
    a.geometry.setRect(10, 10, 10, 10);
    a.z = 100;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), GeometryEventArgs::size);
    GeometryEventArgs b;
    d >> b;
    QCOMPARE(a.geometry, b.geometry);
    QCOMPARE(a.z, b.z);
}

void test_QByteArray::test_ImageInfoEventArgs(){
    ImageInfoEventArgs a;
    a.sizeInBytes = 1000000;
    a.bytesPerLine = 10000;
    a.format = QImage::Format_A2BGR30_Premultiplied;
    QByteArray d;
    d << a;
    QCOMPARE(d.size(), ImageInfoEventArgs::size);
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
    QCOMPARE(d.size(), WaitForPaintEventArgs::size);
    WaitForPaintEventArgs b;
    d >> b;
    QCOMPARE(a.marker, b.marker);
}

QTEST_APPLESS_MAIN(test_QByteArray)

#include "test_QByteArray.moc"
