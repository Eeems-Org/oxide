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

    QCOMPARE(normalize(QPoint(0, 0), QRect(QPoint(0, 0), QPoint(10, 10))), QPointF(0, 0));
    QCOMPARE(normalize(QPoint(5, 5), QRect(QPoint(0, 0), QPoint(10, 10))), QPointF(0.5, 0.5));
    QCOMPARE(normalize(QPoint(10, 10), QRect(QPoint(0, 0), QPoint(10, 10))), QPointF(1, 1));
    QCOMPARE(normalize(QPoint(-10, -10), QRect(QPoint(-10, -10), QPoint(10, 10))), QPointF(0, 0));
    QCOMPARE(normalize(QPoint(0, 0), QRect(QPoint(-10, -10), QPoint(10, 10))), QPointF(0.5, 0.5));
    QCOMPARE(normalize(QPoint(10, 10), QRect(QPoint(-10, -10), QPoint(10, 10))), QPointF(1, 1));

    QCOMPARE(normalize(QPoint(0, 0), QRect(QPoint(0, 0), QPoint(1404, 1872))), QPointF(0, 0));
    QCOMPARE(
        normalize(QPoint(100, 100), QRect(QPoint(0, 0), QPoint(1404, 1872))),
        QPointF(0.0712250712251, 0.0534188034188)
    );
    QCOMPARE(normalize(QPoint(1404, 1872), QRect(QPoint(0, 0), QPoint(1404, 1872))), QPointF(1, 1));
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

DECLARE_TEST(test_Math)
