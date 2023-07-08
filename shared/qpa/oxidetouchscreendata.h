/*
 * Large portions of this file were copied from https://github.com/qt/qtbase/blob/5.15/src/platformsupport/input/evdevtouch/qevdevtouchhandler.cpp
 * As such, this file is under LGPL instead of MIT
 */
#pragma once

#include <QRect>
#include <QTransform>
#include <QMutex>
#include <QTouchEvent>
#include <QHash>
#include <QString>
#include <QPointer>
#include <qpa/qwindowsysteminterface.h>

#include <linux/input.h>

class OxideTouchScreenData{
public:
    OxideTouchScreenData(const QStringList& args);

    void assignIds();
    void processInputEvent(input_event* data);

    int m_lastEventType;
    QList<QWindowSystemInterface::TouchPoint> m_touchPoints;
    QList<QWindowSystemInterface::TouchPoint> m_lastTouchPoints;

    struct Contact {
        int trackingId = -1;
        int x = 0;
        int y = 0;
        int maj = -1;
        int pressure = 0;
        Qt::TouchPointState state = Qt::TouchPointPressed;
        QTouchEvent::TouchPoint::InfoFlags flags;
    };
    QHash<int, Contact> m_contacts; // The key is a tracking id for type A, slot number for type B.
    QHash<int, Contact> m_lastContacts;
    Contact m_currentData;
    int m_currentSlot;

    double m_timeStamp;
    double m_lastTimeStamp;

    int findClosestContact(const QHash<int, Contact>& contacts, int x, int y, int* dist);
    void addTouchPoint(const Contact& contact, Qt::TouchPointStates* combinedStates);
    void reportPoints();
    void loadMultiScreenMappings();

    QRect screenGeometry() const;

    int hw_range_x_min;
    int hw_range_x_max;
    int hw_range_y_min;
    int hw_range_y_max;
    int hw_pressure_min;
    int hw_pressure_max;
    QString hw_name;
    bool m_forceToActiveWindow;
    bool m_typeB;
    QTransform m_rotate;
    bool m_singleTouch;
    mutable QPointer<QScreen> m_screen;
    QTouchDevice* m_device;
};
