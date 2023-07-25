#include "wacom.h"

#include <liboxide/threading.h>
#include <liboxide/devicesettings.h>

#include <QTabletEvent>
#include <QScreen>

Wacom::Wacom()
: QThread(),
  m_device{deviceSettings.getWacomDevicePath(), O_RDONLY | O_NONBLOCK},
  m_maxX{deviceSettings.getWacomWidth()},
  m_maxY{deviceSettings.getWacomHeight()},
  m_minXTilt{deviceSettings.getWacomMinXTilt()},
  m_minYTilt{deviceSettings.getWacomMinYTilt()},
  m_maxXTilt{deviceSettings.getWacomMaxXTilt()},
  m_maxYTilt{deviceSettings.getWacomMaxYTilt()},
  m_maxPressure{deviceSettings.getWacomPressure()}
{
    if(m_device.fd == -1){
        qFatal("failed to open tablet device");
    }
    m_notifier = new QSocketNotifier(m_device.fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Wacom::readData);
    setObjectName("wacom");
    Oxide::startThreadWithPriority(this, QThread::HighPriority);
    moveToThread(this);
}

Wacom::~Wacom(){
    m_device.close();
    requestInterruption();
    quit();
    wait();
}

void Wacom::processEvent(input_event* event){
    if(event->type == EV_ABS){
        switch(event->code){
            case ABS_X:
                state.x = event->value;
                break;
            case ABS_Y:
                state.y = event->value;
                break;
            case ABS_PRESSURE:
                state.pressure = event->value;
                break;
            case ABS_TILT_X:
                state.tiltX = event->value;
                break;
            case ABS_TILT_Y:
                state.tiltY = event->value;
                break;
            default:
                break;
        }
    }else if(event->type == EV_KEY){
        switch(event->code){
            case BTN_TOUCH:
                state.down = event->value != 0;
                break;
            case BTN_TOOL_PEN:
                state.tool = event->value ? QTabletEvent::Pen : 0;
                break;
            case BTN_TOOL_RUBBER:
                state.tool = event->value ? QTabletEvent::Eraser : 0;
                break;
            default:
                break;
        }
    }else if(event->type == EV_SYN && event->code == SYN_REPORT && m_lastEventType != EV_SYN){
        report();
    }
    m_lastEventType = event->type;
}

void Wacom::report(){
    qreal x = state.x / qreal(m_maxX);
    qreal y = state.y / qreal(m_maxY);
    qreal pressure = state.pressure / qreal(m_maxPressure);
    // Raw axis need to be inverted on a rM1
    int tiltX = Oxide::Math::convertRange(state.tiltY, deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt(), -90, 90);
    int tiltY = Oxide::Math::convertRange(state.tiltX, deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMaxXTilt(), -90, 90);
    QRect winRect = QGuiApplication::primaryScreen()->geometry();
    QPointF globalPos(x * winRect.width(), y * winRect.height());
    int pointer = state.tool;
    if(!state.down && state.lastReportDown){
        globalPos = state.lastReportPos;
        pointer = state.lastReportTool;
    }
    O_DEBUG(state.pressure << pressure << (unsigned int)ceil(qreal(pressure) * deviceSettings.getWacomPressure()));
    if(!state.lastReportTool && state.tool){
        QGuiApplication::postEvent(qApp, new QTabletEvent(
            QTabletEvent::TabletEnterProximity,
            globalPos,
            globalPos,
            0, // deviceid
            pointer,
            pressure,
            tiltX,
            tiltY,
            0, // tangentialPressure
            0, // rotation
            0, // z
            qApp->keyboardModifiers(),
            0, // uniqueid
            state.down ? Qt::LeftButton : Qt::NoButton,
            qApp->mouseButtons()
        ));
    }

    QEvent::Type type = QTabletEvent::TabletMove;
    if(state.down && !state.lastReportDown){
        type = QTabletEvent::TabletPress;
    }else if(!state.down && state.lastReportDown){
        type = QTabletEvent::TabletRelease;
    }
    QGuiApplication::postEvent(qApp, new QTabletEvent(
        type,
        globalPos,
        globalPos,
        0, // deviceid
        pointer,
        pressure,
        tiltX,
        tiltY,
        0, // tangentialPressure
        0, // rotation
        0, // z
        qApp->keyboardModifiers(),
        0, // uniqueid
        state.down ? Qt::LeftButton : Qt::NoButton,
        qApp->mouseButtons()
    ));
    if(state.lastReportTool && !state.tool){
        QGuiApplication::postEvent(qApp, new QTabletEvent(
            QTabletEvent::TabletLeaveProximity,
            globalPos,
            globalPos,
            0, // deviceid
            pointer,
            pressure,
            tiltX,
            tiltY,
            0, // tangentialPressure
            0, // rotation
            0, // z
            qApp->keyboardModifiers(),
            0, // uniqueid
            state.down ? Qt::LeftButton : Qt::NoButton,
            qApp->mouseButtons()
        ));
    }
    state.lastReportDown = state.down;
    state.lastReportTool = state.tool;
    state.lastReportPos = globalPos;
}

void Wacom::readData(){
    forever{
        if(isInterruptionRequested()){
            return;
        }
        input_event event;
        auto res = read(m_device.fd, &event, sizeof(event));
        if(res == 0){
            return;
        }
        if(res > 0){
            processEvent(&event);
            continue;
        }
        if(errno != EAGAIN){
            O_WARNING("Failed to read tablet device:" << strerror(errno));
            return;
        }
        return;
    }
}
