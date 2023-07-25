#pragma once
#include "autotest.h"
#include <QBuffer>

class test_QByteArray : public QObject{
    Q_OBJECT

public:
    test_QByteArray();
    ~test_QByteArray();

private slots:
    void init();
    void cleanup();
    void test_qint8();
    void test_qint16();
    void test_qint32();
    void test_qint64();
    void test_quint8();
    void test_quint16();
    void test_quint32();
    void test_quint64();
    void test_TouchEventPoint();
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
};
