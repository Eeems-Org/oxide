/*
 * Large portions of this file were copied from https://github.com/qt/qtbase/blob/5.15/src/platformsupport/input/evdevtablet/qevdevtablethandler.cpp
 * As such, this file is under LGPL instead of MIT
 */
#include "oxidetabletdata.h"

#include <QTabletEvent>
#include <QDebug>
#include <QGuiApplication>
#include <qpa/qwindowsysteminterface.h>
#include <private/qhighdpiscaling_p.h>
#include <liboxide.h>

OxideTabletData::OxideTabletData(int fd) : fd(fd), lastEventType(0){
    memset(&minValues, 0, sizeof(minValues));
    memset(&maxValues, 0, sizeof(maxValues));
    memset(static_cast<void *>(&state), 0, sizeof(state));
    maxValues.p = deviceSettings.getWacomPressure();
    maxValues.x = deviceSettings.getWacomWidth();
    maxValues.y = deviceSettings.getWacomHeight();
}

void OxideTabletData::processInputEvent(input_event* ev){
    if(ev->type == EV_ABS){
        switch (ev->code){
        case ABS_X:
            state.x = ev->value;
            break;
        case ABS_Y:
            state.y = ev->value;
            break;
        case ABS_PRESSURE:
            state.p = ev->value;
            break;
        case ABS_DISTANCE:
            state.d = ev->value;
            break;
        default:
            break;
        }
    }else if(ev->type == EV_KEY){
        // code BTN_TOOL_* value 1 -> proximity enter
        // code BTN_TOOL_* value 0 -> proximity leave
        // code BTN_TOUCH value 1 -> contact with screen
        // code BTN_TOUCH value 0 -> no contact
        switch (ev->code){
        case BTN_TOUCH:
            state.down = ev->value != 0;
            break;
        case BTN_TOOL_PEN:
            state.tool = ev->value ? int(QTabletEvent::Pen) : 0;
            break;
        case BTN_TOOL_RUBBER:
            state.tool = ev->value ? int(QTabletEvent::Eraser) : 0;
            break;
        default:
            break;
        }
    }else if(ev->type == EV_SYN && ev->code == SYN_REPORT && lastEventType != ev->type){
        report();
    }
    lastEventType = ev->type;
}

QRect OxideTabletData::screenGeometry() const{
    QScreen* screen = QGuiApplication::primaryScreen();
    return QHighDpi::toNativePixels(screen->geometry(), screen);
}

void OxideTabletData::report(){
    if(!state.lastReportTool && state.tool){
        QWindowSystemInterface::handleTabletEnterProximityEvent(QTabletEvent::Stylus, state.tool, fd);
    }

    qreal nx = (state.x - minValues.x) / qreal(maxValues.x - minValues.x);
    qreal ny = (state.y - minValues.y) / qreal(maxValues.y - minValues.y);

    QPointF globalPos(nx * screenGeometry().width(), ny * screenGeometry().height());
    int pointer = state.tool;
    // Prevent sending confusing values of 0 when moving the pen outside the active area.
    if(!state.down && state.lastReportDown){
        globalPos = state.lastReportPos;
        pointer = state.lastReportTool;
    }
    int pressureRange = maxValues.p - minValues.p;
    qreal pressure = pressureRange ? (state.p - minValues.p) / qreal(pressureRange) : qreal(1);
    if(state.down || state.lastReportDown){
        QWindowSystemInterface::handleTabletEvent(
            nullptr,
            QPointF(),
            globalPos,
            int(QTabletEvent::Stylus),
            pointer,
            state.down ? Qt::LeftButton : Qt::NoButton,
            pressure,
            0,
            0,
            0,
            0,
            0,
            fd,
            qGuiApp->keyboardModifiers()
        );
    }
    if(state.lastReportTool && !state.tool){
        QWindowSystemInterface::handleTabletLeaveProximityEvent(int(QTabletEvent::Stylus), state.tool, fd);
    }
    state.lastReportDown = state.down;
    state.lastReportTool = state.tool;
    state.lastReportPos = globalPos;
}
