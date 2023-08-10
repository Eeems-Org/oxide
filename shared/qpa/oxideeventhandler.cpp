#include "oxideeventhandler.h"
#include "oxideintegration.h"

#include <QGuiApplication>
#include <private/qhighdpiscaling_p.h>
#include <liboxide/devicesettings.h>
#include <liboxide/threading.h>
#include <liboxide/math.h>

OxideEventHandler::OxideEventHandler(QLocalSocket* socket)
: QObject(),
  m_socket{socket},
  m_tabletPenDown{false}
{
    m_tabletTransform = QTransform::fromTranslate(0.5, 0.5).rotate(90).translate(-0.5, -0.5);
    m_touchscreen = new QTouchDevice();
    m_touchscreen->setName("oxide");
    m_touchscreen->setType(QTouchDevice::TouchScreen);
    m_touchscreen->setCapabilities(QTouchDevice::Position | QTouchDevice::NormalizedPosition | QTouchDevice::Area | QTouchDevice::Pressure);
    if(deviceSettings.getTouchPressure()){
        m_touchscreen->setCapabilities(m_touchscreen->capabilities() | QTouchDevice::Pressure);
    }
    m_touchscreen->setMaximumTouchPoints(deviceSettings.getTouchSlots());
    QWindowSystemInterface::registerTouchDevice(m_touchscreen);
    connect(m_socket, &QLocalSocket::readyRead, this, &OxideEventHandler::readEvents);
    moveToThread(socket->thread());
    connect(qApp, &QGuiApplication::aboutToQuit, this, &OxideEventHandler::deleteLater);
}

OxideEventHandler::~OxideEventHandler(){ }

void OxideEventHandler::readEvents(){
    QMutexLocker locker(&m_eventMutex);
    Q_UNUSED(locker);
    while(!m_socket->atEnd()){
        auto event = Oxide::Tarnish::WindowEvent::fromSocket(m_socket);
        switch(event.type){
            case Oxide::Tarnish::Geometry:{
                auto primaryScreen = OxideIntegration::instance()->primaryScreen();
                auto geometry = event.geometryData.geometry();
                primaryScreen->setGeometry(geometry);
                auto window = primaryScreen->topWindow();
                if(window != nullptr){
                    window->setGeometry(geometry);
                }
                primaryScreen->setDirty(geometry);
                primaryScreen->scheduleUpdate();
                break;
            }
            case Oxide::Tarnish::Raise:{
                if(QCoreApplication::closingDown()){
                    break;
                }
                auto primaryScreen = OxideIntegration::instance()->primaryScreen();
                auto window = primaryScreen->topWindow();
                if(window != nullptr){
                    window->raise();
                }
                break;
            }
            case Oxide::Tarnish::Lower:{
                if(QCoreApplication::closingDown()){
                    break;
                }
                auto primaryScreen = OxideIntegration::instance()->primaryScreen();
                auto window = primaryScreen->topWindow();
                if(window != nullptr){
                    window->lower();
                }
                break;
            }
            case Oxide::Tarnish::Close:{
                if(QCoreApplication::closingDown()){
                    break;
                }
                qApp->quit();
                break;
            }
            case Oxide::Tarnish::Ping:
                event.toSocket(m_socket);
                break;
            case Oxide::Tarnish::WaitForPaint:
                if(QCoreApplication::closingDown()){
                    break;
                }
                QMetaObject::invokeMethod(
                    OxideIntegration::instance()->primaryScreen(),
                    "waitForPaint",
                    Qt::QueuedConnection,
                    Q_ARG(unsigned int, event.waitForPaintData.marker)
                );
                break;
            case Oxide::Tarnish::Key: handleKey(&event.keyData); break;
            case Oxide::Tarnish::Touch: handleTouch(&event.touchData); break;
            case Oxide::Tarnish::Tablet: handleTablet(&event.tabletData); break;
            case Oxide::Tarnish::ImageInfo:
            case Oxide::Tarnish::Repaint:
            case Oxide::Tarnish::FrameBuffer:
            case Oxide::Tarnish::Invalid:
            default:
                break;
        }
    }
}

QWindowSystemInterface::TouchPoint OxideEventHandler::getTouchPoint(const Oxide::Tarnish::TouchEventPoint& data){
    QWindowSystemInterface::TouchPoint point;
    point.id = data.id;
    if(data.tool != Oxide::Tarnish::Finger){
        point.flags = point.flags | QTouchEvent::TouchPoint::Token;
    }
    switch(data.state){
        case Oxide::Tarnish::PointPress:
            point.state = Qt::TouchPointPressed;
            break;
        case Oxide::Tarnish::PointMove:
            point.state = Qt::TouchPointMoved;
            break;
        case Oxide::Tarnish::PointRelease:
            point.state = Qt::TouchPointReleased;
            break;
        case Oxide::Tarnish::PointStationary:
            point.state = Qt::TouchPointStationary;
            break;
        default:
            Q_ASSERT_X(false, "TouchEventPoint.state", QString("State is invalid: %1").arg(data.state).toStdString().c_str());
    }
    auto primaryScreen = OxideIntegration::instance()->primaryScreen();
    QRect winRect = QHighDpi::toNativePixels(primaryScreen->geometry(), primaryScreen);
    auto rawPosition = QPoint((data.x * winRect.width()) + winRect.x(), (data.y * winRect.height()) + winRect.y());
    point.area = QRectF(0, 0, data.width, data.height);
    point.area.moveCenter(rawPosition);
    point.pressure = data.pressure;
    point.normalPosition = data.point();
    point.rawPositions.append(rawPosition);
    return point;
}

void OxideEventHandler::handleTouch(Oxide::Tarnish::TouchEventArgs* data){
    if(QCoreApplication::closingDown()){
        return;
    }
    auto primaryScreen = OxideIntegration::instance()->primaryScreen();
    QRect winRect = QHighDpi::toNativePixels(primaryScreen->geometry(), primaryScreen);
    if(winRect.isNull()){
        O_WARNING("Null screenGeometry");
        return;
    }
    m_lastTouchPoints = m_touchPoints;
    m_touchPoints.clear();
    for(const auto& point : data->points){
        auto touchPoint = getTouchPoint(point);
        m_touchPoints.append(touchPoint);
    }
    if(qEnvironmentVariableIsSet("OXIDE_QPA_DEBUG_EVENTS")){
        qDebug() << m_touchPoints;
    }
    QWindowSystemInterface::handleTouchEvent(nullptr, m_touchscreen, m_touchPoints);
}

void OxideEventHandler::handleTablet(Oxide::Tarnish::TabletEventArgs* data){
    if(QCoreApplication::closingDown()){
        return;
    }
    if(data->type == Oxide::Tarnish::PenEnterProximity){
        QWindowSystemInterface::handleTabletEnterProximityEvent(
            QTabletEvent::Stylus,
            data->tool == Oxide::Tarnish::Pen ? QTabletEvent::Pen : QTabletEvent::Eraser,
            int(QTabletEvent::Stylus)
        );
        return;
    }
    if(data->type == Oxide::Tarnish::PenLeaveProximity){
        QWindowSystemInterface::handleTabletLeaveProximityEvent(
            QTabletEvent::Stylus,
            data->tool == Oxide::Tarnish::Pen ? QTabletEvent::Pen : QTabletEvent::Eraser,
            int(QTabletEvent::Stylus)
        );
        return;
    }
    switch(data->type){
        case Oxide::Tarnish::PenPress:
            m_tabletPenDown = true;
            break;
        case Oxide::Tarnish::PenUpdate:
            if(!m_tabletPenDown && !qEnvironmentVariableIsSet("OXIDE_QPA_HOVER_EVENTS")){
                return;
            }
            break;
        case Oxide::Tarnish::PenRelease:
            m_tabletPenDown = false;
            break;
        default:
            break;
    }
    auto pos = data->point();
    // TODO - For some reason when the geometry of the screen is offset, the position is being tracked
    //        as if it's in that coordinate system, even though tablet events have a bottom right origin
    //        and not a top left origin. This means leads to odd behavior, like only clicks on the far
    //        right in the task switcher
    if(qEnvironmentVariableIsSet("OXIDE_QPA_DEBUG_EVENTS")){
        auto screen = QGuiApplication::screenAt(pos);
        auto window = QGuiApplication::topLevelAt(pos);
        qDebug() << (m_tabletPenDown ? "DOWN" : "UP") << pos
            << screen << (screen != nullptr ? screen->geometry() : QRect())
            << window << (window != nullptr ? window->geometry() : QRect());
    }
    QWindowSystemInterface::handleTabletEvent(
        nullptr,
        QPointF(),
        pos,
        int(QTabletEvent::Stylus),
        data->tool == Oxide::Tarnish::Pen ? QTabletEvent::Pen : QTabletEvent::Eraser,
        m_tabletPenDown ? Qt::LeftButton : Qt::NoButton,
        data->pressure,
        data->tiltX,
        data->tiltY,
        0,
        0,
        0,
        Oxide::Tarnish::getEventPipeFd(),
        qGuiApp->keyboardModifiers()
    );
//    QWindow *window,
//    const QPointF &local,
//    const QPointF &global,
//    int device,
//    int pointerType,
//    Qt::MouseButtons buttons,
//    qreal pressure,
//    int xTilt,
//    int yTilt,
//    qreal tangentialPressure,
//    qreal rotation,
//    int z,
//    qint64 uid,
//    Qt::KeyboardModifiers modifiers = Qt::NoModifier
}

void OxideEventHandler::handleKey(Oxide::Tarnish::KeyEventArgs* data){
    if(QCoreApplication::closingDown()){
        return;
    }
    QEvent::Type type = QEvent::None;
    bool repeat = false;
    switch(data->type){
        case Oxide::Tarnish::ReleaseKey:
            type = QEvent::KeyRelease;
            break;
        case Oxide::Tarnish::PressKey:
            type = QEvent::KeyPress;
            break;
        case Oxide::Tarnish::RepeatKey:
            type = QEvent::KeyPress;
            repeat = true;
            break;
    }
    // TODO - sort out how to get key modifiers state
    QWindowSystemInterface::handleKeyEvent(
        nullptr,
        type,
        data->code,
        Qt::NoModifier,
        QString((unsigned char)data->unicode),
        repeat
    );
//    QWindow *window,
//    QEvent::Type t,
//    int k,
//    Qt::KeyboardModifiers mods,
//    const QString & text = QString(),
//    bool autorep = false,
//    ushort count = 1
}

