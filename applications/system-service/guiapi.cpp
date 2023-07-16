#include "guiapi.h"
#include "appsapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"
#include "dbusservice.h"
#include "repaintnotifier.h"
#include "eventlistener.h"

#define W_WARNING(msg) O_WARNING(__PRETTY_FUNCTION__ << msg << getSenderPgid())
#define W_DEBUG(msg) O_DEBUG(__PRETTY_FUNCTION__ << msg << getSenderPgid())
#define W_DENIED() W_DEBUG("DENY")
#define W_ALLOWED() W_DEBUG("ALLOW")

Q_DECLARE_METATYPE(EPFrameBuffer::WaveformMode)

static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));

static GUIThread m_thread;

GuiAPI* GuiAPI::__singleton(GuiAPI* self){
    static GuiAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}
GuiAPI::GuiAPI(QObject* parent)
: APIBase(parent),
  m_enabled(false)
{
    qRegisterMetaType<EPFrameBuffer::WaveformMode>();
    qRegisterMetaType<QAbstractSocket::SocketState>();
    Oxide::Sentry::sentry_transaction("gui", "init", [this](Oxide::Sentry::Transaction* t){
        Q_UNUSED(t);
        m_screenGeometry = deviceSettings.screenGeometry();
        __singleton(this);
        connect(touchHandler, &DigitizerHandler::inputEvent, this, &GuiAPI::touchEvent);
        connect(wacomHandler, &DigitizerHandler::inputEvent, this, &GuiAPI::tabletEvent);
        connect(buttonHandler, &ButtonHandler::rawEvent, this, &GuiAPI::keyEvent);
        connect(&m_repaintNotifier, &RepaintNotifier::windowRepainted, this, &GuiAPI::repainted);
        eventListener->append([this](QObject* object, QEvent* event){
            Q_UNUSED(object)
            switch(event->type()){
                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                    writeKeyEvent(event);
                    return true;
                case QEvent::TouchBegin:
                case QEvent::TouchCancel:
                case QEvent::TouchEnd:
                case QEvent::TouchUpdate:
                    writeTouchEvent(event);
                    return true;
                case QEvent::TabletEnterProximity:
                case QEvent::TabletLeaveProximity:
                case QEvent::TabletPress:
                case QEvent::TabletMove:
                case QEvent::TabletRelease:
                case QEvent::TabletTrackingChange:
                    writeTabletEvent(event);
                    return true;
                default:
                    return false;
            }
        });
    });
}
GuiAPI::~GuiAPI(){
    while(!m_windows.isEmpty()){
        auto window = m_windows.take(m_windows.firstKey());
        delete window;
    }
}

void GuiAPI::startup(){
    W_DEBUG("Startup");
    m_thread.m_repaintNotifier = &m_repaintNotifier;
    m_thread.m_screenGeometry = &m_screenGeometry;
    m_thread.start();
    Oxide::startThreadWithPriority(&m_thread, QThread::TimeCriticalPriority);
}

void GuiAPI::shutdown(){
    W_DEBUG("Shutdown");
    while(!m_windows.isEmpty()){
        auto window = m_windows.take(m_windows.firstKey());
        window->_close();
    }
    emit m_repaintNotifier.windowRepainted(nullptr, std::numeric_limits<unsigned int>::max());
    m_thread.quit();
}

QRect GuiAPI::geometry(){
    if(!hasPermission()){
        W_DENIED();
        return QRect();
    }
    W_ALLOWED();
    return m_screenGeometry;
}

QRect GuiAPI::_geometry(){ return m_screenGeometry; }

void GuiAPI::setEnabled(bool enabled){
    qDebug() << "GUI API" << enabled;
    m_enabled = enabled;
    for(auto window : m_windows){
        if(window != nullptr){
            window->setEnabled(enabled);
        }
    }
}

bool GuiAPI::isEnabled(){ return m_enabled; }

Window* GuiAPI::_createWindow(QRect geometry, QImage::Format format){
    auto id = QUuid::createUuid().toString(QUuid::Id128);
    auto path = QString(OXIDE_SERVICE_PATH) + "/window/" + QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
    auto pgid = getSenderPgid();
    auto window = new Window(id, path, pgid, geometry, m_windows.count(), format);
    m_windows.insert(path, window);
    sortWindows();
    connect(window, &Window::closed, this, [this, window, path]{
        if(m_windows.remove(path)){
            sortWindows();
        }
        auto region = window->_geometry().intersected(m_screenGeometry.translated(-m_screenGeometry.topLeft()));
        m_thread.enqueue(nullptr, region, EPFrameBuffer::Initialize, 0, true);
    });
    connect(window, &Window::raised, this, [this, window]{
        auto windows = sortedWindows();
        int z = 0;
        for(auto w : windows){
            if(w == window){
                w->setZ(z++);
            }
        }
        window->setZ(z);
    });
    connect(window, &Window::lowered, this, [this, window]{
        auto windows = sortedWindows();
        window->setZ(0);
        int z = 1;
        for(auto w : windows){
            if(w != window){
                w->setZ(z++);
            }
        }
    });
    for(auto item : appsAPI->runningApplicationsNoSecurityCheck().values()){
        Application* app = appsAPI->getApplication(item.value<QDBusObjectPath>());
        if(app->processId() == pgid){
            connect(app, &Application::paused, window, [=]{
                window->setVisible(false);
            });
            connect(app, &Application::resumed, window, [=]{
                window->setVisible(true);
            });
        }
    }
    window->setEnabled(m_enabled);
    return window;
}

QDBusObjectPath GuiAPI::createWindow(int x, int y, int width, int height, int format){
    if(!hasPermission()){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    Window* window = _createWindow(QRect(x, y, width, height), (QImage::Format)format);
    return window == nullptr ? QDBusObjectPath("/") : window->path();
}

QDBusObjectPath GuiAPI::createWindow(QRect geometry, int format){
    if(!hasPermission()){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    Window* window = _createWindow(geometry, (QImage::Format)format);
    return window == nullptr ? QDBusObjectPath("/") : window->path();
}

QDBusObjectPath GuiAPI::createWindow(int format){
    if(!hasPermission()){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    Window* window = _createWindow(m_screenGeometry, (QImage::Format)format);
    return window == nullptr ? QDBusObjectPath("/") : window->path();
}

QList<QDBusObjectPath> GuiAPI::windows(){
    auto pgid = getSenderPgid();
    QList<QDBusObjectPath> windows;
    for(auto window : m_windows){
        if(window->pgid() == pgid){
            windows.append(window->path());
        }
    }
    return windows;
}

void GuiAPI::repaint(){
    if(!APIBase::hasPermission("gui")){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    m_thread.enqueue(nullptr, m_screenGeometry, EPFrameBuffer::HighQualityGrayscale, 0, true);
    waitForLastUpdate();
}

bool GuiAPI::isThisPgId(pid_t valid_pgid){
    if(!calledFromDBus()){
        return true;
    }
    auto pgid = getSenderPgid();
    return pgid == valid_pgid || pgid == getpid();
}

QMap<QString, Window*> GuiAPI::allWindows(){ return m_windows; }

QList<Window*> GuiAPI::sortedWindows(){
    auto sortedWindows = m_windows.values();
    std::sort(sortedWindows.begin(), sortedWindows.end());
    return sortedWindows;
}

void GuiAPI::closeWindows(pid_t pgid){
    for(auto window : m_windows.values()){
        if(window->pgid() == pgid){
            window->_close();
        }
    }
}

void GuiAPI::waitForLastUpdate(){
    QMutexLocker locker(&m_thread.m_mutex);
    if(!m_thread.active()){
        locker.unlock();
        return;
    }
    // TODO - Should there be a timeout?
    QEventLoop loop;
    QMetaObject::Connection conn;
    conn = QObject::connect(&m_repaintNotifier, &RepaintNotifier::windowRepainted, [&loop, &conn](Window* window, unsigned int marker){
        Q_UNUSED(window)
        Q_UNUSED(marker);
        QObject::disconnect(conn);
        loop.exit();
    });
    locker.unlock();
    loop.exec();
}

void GuiAPI::dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker){
    Q_ASSERT(window != nullptr);
    m_thread.enqueue(window, region, waveform, marker);
}

GUIThread* GuiAPI::guiThread(){ return &m_thread; }

void GuiAPI::writeTouchEvent(QEvent* event){
    W_DEBUG(event);
    TouchEventArgs args;
    switch(event->type()){
        case QEvent::TouchBegin:
            args.type = TouchEventType::TouchPress;
            break;
        case QEvent::TouchEnd:
            args.type = TouchEventType::TouchRelease;
            break;
        case QEvent::TouchUpdate:
            args.type = TouchEventType::TouchUpdate;
            break;
        case QEvent::TouchCancel:
            args.type = TouchEventType::TouchCancel;
            break;
        default:
            return;
    }
    auto touchEvent = static_cast<QTouchEvent*>(event);
    for(auto point : touchEvent->touchPoints()){
        TouchEventPointState state = TouchEventPointState::PointStationary;
        switch(point.state()){
            case Qt::TouchPointPressed:
                state = TouchEventPointState::PointPress;
                break;
            case Qt::TouchPointMoved:
                state = TouchEventPointState::PointMove;
                break;
            case Qt::TouchPointReleased:
                state = TouchEventPointState::PointRelease;
                break;
            case Qt::TouchPointStationary:
                break;
        }
        args.points.push_back(TouchEventPoint{
            .id = (int)point.uniqueId().numericId(),
            .state = state,
            .x = point.normalizedPos().x(),
            .y = point.normalizedPos().y(),
            .width = point.ellipseDiameters().width(),
            .height = point.ellipseDiameters().height(),
            .tool = TouchEventTool::Finger,
            .pressure = point.pressure(),
            .rotation = point.rotation(),
        });
        W_DEBUG(point.normalizedPos() << point.ellipseDiameters());
    }
    for(auto window : m_windows){
        if(!window->_isVisible() || window->isAppPaused()){
            continue;
        }
        // TODO - adjust position and filter if it doesn't apply
        window->writeEvent(args);
    }
}

void GuiAPI::writeTabletEvent(QEvent* event){
    W_DEBUG(event);
    TabletEventType type;
    switch(event->type()){
        case QEvent::TabletPress:
            type = TabletEventType::PenPress;
            break;
        case QEvent::TabletMove:
            type = TabletEventType::PenUpdate;
            break;
        case QEvent::TabletRelease:
            type = TabletEventType::PenRelease;
            break;
        case QEvent::TabletEnterProximity:
        case QEvent::TabletLeaveProximity:
        case QEvent::TabletTrackingChange:
            // TODO - implement these in the protocol
        default:
            return;
    }
    TabletEventTool tool = TabletEventTool::Pen;
    auto tabletEvent = static_cast<QTabletEvent*>(event);
    switch(tabletEvent->pointerType()){
        case QTabletEvent::Pen:
            tool = TabletEventTool::Pen;
            break;
        case QTabletEvent::Eraser:
            tool = TabletEventTool::Eraser;
            break;
        case QTabletEvent::UnknownPointer:
        case QTabletEvent::Cursor:
            // TODO - implement these in the protocol
            return;
    }
    TabletEventArgs args{
        .type = type,
        .tool = tool,
        .x = tabletEvent->pos().x(),
        .y = tabletEvent->pos().y(),
        .pressure = (unsigned int)tabletEvent->pressure(),
        .tiltX = tabletEvent->xTilt(),
        .tiltY = tabletEvent->yTilt(),
    };
    for(auto window : m_windows){
        if(!window->_isVisible() || window->isAppPaused()){
            continue;
        }
        // TODO - adjust position and filter if it doesn't apply
        window->writeEvent(args);
    }
}

void GuiAPI::writeKeyEvent(QEvent* event){
    W_DEBUG(event);
    auto keyEvent = static_cast<QKeyEvent*>(event);
    auto text = keyEvent->text().left(0);
    KeyEventArgs args{
        .code = (unsigned int)keyEvent->key(),
        .type = event->type() == QEvent::KeyPress
            ? (
                  keyEvent->isAutoRepeat()
                      ? KeyEventType::RepeatKey
                      : KeyEventType::PressKey
            )
            : KeyEventType::ReleaseKey,
        .unicode = (unsigned int)(text.length() ? text.at(0).unicode() : 0),
    };
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeEvent(args);
        }
    }
}

void GuiAPI::touchEvent(const input_event& event){
    O_EVENT(event);
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeTouchEvent(event);
        }
    }
}

void GuiAPI::tabletEvent(const input_event& event){
    O_EVENT(event);
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeTabletEvent(event);
        }
    }
}

void GuiAPI::keyEvent(const input_event& event){
    O_EVENT(event);
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeKeyEvent(event);
        }
    }
}

void GuiAPI::repainted(Window* window, unsigned int marker){
    if(window != nullptr){
        QMetaObject::invokeMethod(
            &window->m_repaintNotifier,
            "repainted",
            Qt::AutoConnection,
            Q_ARG(unsigned int, marker)
        );
    }
}

void GuiAPI::sortWindows(){
    auto windows = sortedWindows();
    int z = 0;
    for(auto window : windows){
        window->setZ(z++);
    }
}

bool GuiAPI::hasPermission(){
    pid_t pgid = getSenderPgid();
    if(dbusService->isChildGroup(pgid)){
        return true;
    }
    return pgid == ::getpgrp();
}
