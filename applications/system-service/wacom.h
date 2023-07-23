#pragma once
#include <QThread>

#include <liboxide/event_device.h>

#include "systemapi.h"

class Wacom : public QThread{
public:
    Wacom();
    ~Wacom();
    void processEvent(input_event* event);
    void report();
    void readData();

private:
    QSocketNotifier* m_notifier;
    Oxide::event_device m_device;
    int m_lastEventType;
    int m_maxX;
    int m_maxY;
    int m_minXTilt;
    int m_minYTilt;
    int m_maxXTilt;
    int m_maxYTilt;
    int m_maxPressure;
    struct{
        int x;
        int y;
        int pressure;
        int tiltX;
        int tiltY;
        bool down;
        bool lastReportDown;
        int tool;
        int lastReportTool;
        QPointF lastReportPos;
    } state;
};
