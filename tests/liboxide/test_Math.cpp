#include "test_Math.h"

#include <liboxide/math.h>
#include <liboxide/devicesettings.h>

using namespace Oxide::Math;

test_Math::test_Math(){ }
test_Math::~test_Math(){ }

void test_Math::test_normalize(){
    QCOMPARE(normalize(0, 0, 10), 0);
    QCOMPARE(normalize(5, 0, 10), 0.5);
    QCOMPARE(normalize(10, 0, 10), 1);

    QCOMPARE(normalize(deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt()), 0);
    QCOMPARE(normalize(deviceSettings.getWacomMaxYTilt(), deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt()), 1);

    QCOMPARE(normalize(QPoint(0, 0), rect(0, 0, 10, 10)), QPointF(0, 0));
    QCOMPARE(normalize(QPoint(5, 5), rect(0, 0, 10, 10)), QPointF(0.5, 0.5));
    QCOMPARE(normalize(QPoint(10, 10), rect(0, 0, 10, 10)), QPointF(1, 1));
    QCOMPARE(normalize(QPoint(-10, -10), rect(-10, -10, 10, 10)), QPointF(0, 0));
    QCOMPARE(normalize(QPoint(0, 0), rect(-10, -10, 10, 10)), QPointF(0.5, 0.5));
    QCOMPARE(normalize(QPoint(10, 10), rect(-10, -10, 10, 10)), QPointF(1, 1));

    QCOMPARE(normalize(QPoint(0, 0), rect(0, 0, 1404, 1872)), QPointF(0, 0));
    QCOMPARE(
        normalize(QPoint(100, 100), rect(0, 0, 1404, 1872)),
        QPointF(0.0712250712251, 0.0534188034188)
    );
    QCOMPARE(normalize(QPoint(1404, 1872), rect(0, 0, 1404, 1872)), QPointF(1, 1));
    QCOMPARE(normalize(QPoint(1404, 1872), rect(0, 0, 1404, 1872)), QPointF(1, 1));

    QCOMPARE(normalize(rect(0, 0, 1, 1), rect(0, 0, 10, 10)), rectF(0, 0, 0.1, 0.1));
    QCOMPARE(normalize(rect(4, 4, 6, 6), rect(0, 0, 10, 10)), rectF(0.4, 0.4, 0.6, 0.6));
    QCOMPARE(normalize(rect(9, 9, 10, 10), rect(0, 0, 10, 10)), rectF(0.9, 0.9, 1, 1));

    QCOMPARE(normalize(QPoint(0, 0), QRect(0, 0, 1404, 1872)), QPointF(0, 0));
}

void test_Math::test_convertRange(){
    QCOMPARE(convertRange(-9000, -9000, 9000, -90, 90), -90);
    QCOMPARE(convertRange(0, -9000, 9000, -90, 90), 0);
    QCOMPARE(convertRange(9000, -9000, 9000, -90, 90), 90);

    QCOMPARE(convertRange(-10, -10, 10, -100, 100), -100);
    QCOMPARE(convertRange(0, -10, 10, -100, 100), 0);
    QCOMPARE(convertRange(10, -10, 10, -100, 100), 100);

    QCOMPARE(convertRange(10, 10, 20, -100, 100), -100);
    QCOMPARE(convertRange(15, 10, 20, -100, 100), 0);
    QCOMPARE(convertRange(20, 10, 20, -100, 100), 100);

    QCOMPARE(convertRange(-10, -10, 10, 10, 20), 10);
    QCOMPARE(convertRange(0, -10, 10, 10, 20), 15);
    QCOMPARE(convertRange(10, -10, 10, 10, 20), 20);

    QCOMPARE(convertRange(deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt(), -90, 90), -90);
    QCOMPARE(convertRange(deviceSettings.getWacomMaxYTilt(), deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt(), -90, 90), 90);

    QCOMPARE(convertRange(deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMaxXTilt(), -90, 90), -90);
    QCOMPARE(convertRange(deviceSettings.getWacomMaxXTilt(), deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMaxXTilt(), -90, 90), 90);
}

QRect test_Math::rect(int x0, int y0, int x1, int y1){ return QRect(QPoint(x0, y0), QPoint(x1, y1)); }

QRectF test_Math::rectF(double x0, double y0, double x1, double y1){ return QRectF(QPointF(x0, y0), QPointF(x1, y1)); }

DECLARE_TEST(test_Math)
