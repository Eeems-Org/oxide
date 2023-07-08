/*
 * Large portions of this file were copied from https://github.com/qt/qtbase/blob/5.15/src/platformsupport/input/evdevtablet/qevdevtablethandler.cpp
 * As such, this file is under LGPL instead of MIT
 */
#pragma once

#include <QString>
#include <QPointF>
#include <QRect>

#include <linux/input.h>

class OxideTabletData{
public:
    OxideTabletData(int fd);

    void processInputEvent(input_event* ev);
    QRect screenGeometry() const;
    void report();

    qint64 fd;
    int lastEventType;
    struct {
        int x, y, p;
    } minValues, maxValues;
    struct {
        int x, y, p, d;
        bool down, lastReportDown;
        int tool, lastReportTool;
        QPointF lastReportPos;
    } state;
};
