#include "guiapi.h"
#include "appsapi.h"
#include "digitizerhandler.h"
#include "dbusservice.h"
#include "eventlistener.h"

#include <experimental/random>

#define W_WARNING(msg) O_WARNING(msg << getSenderPgid())
#define W_DEBUG(msg) O_DEBUG(msg << getSenderPgid())
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
  m_enabled(false),
  m_currentMarker{0}
{
    qRegisterMetaType<EPFrameBuffer::WaveformMode>();
    qRegisterMetaType<QAbstractSocket::SocketState>();
    Oxide::Sentry::sentry_transaction("gui", "init", [this](Oxide::Sentry::Transaction* t){
        Q_UNUSED(t);
        m_screenGeometry = deviceSettings.screenGeometry();
        __singleton(this);
        connect(touchHandler, &DigitizerHandler::inputEvent, this, &GuiAPI::touchEvent);
        connect(wacomHandler, &DigitizerHandler::inputEvent, this, &GuiAPI::tabletEvent);
    });
}
GuiAPI::~GuiAPI(){
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    while(!m_windows.isEmpty()){
        auto window = m_windows.take(m_windows.firstKey());
        delete window;
    }
}

void GuiAPI::startup(){
    O_INFO("Starting up GUI API");
    m_thread.m_screenGeometry = &m_screenGeometry;
    Oxide::startThreadWithPriority(&m_thread, QThread::TimeCriticalPriority);
}

void GuiAPI::shutdown(){
    O_INFO("Closing all remaining windows");
    while(!m_windows.isEmpty()){
        m_windows.first()->_close();
    }
    m_thread.shutdown();
    O_INFO("GUI API shutdown complete");
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
    O_INFO("GUI API" << enabled);
    m_enabled = enabled;
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
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
    m_windowMutex.lock();
    auto window = new Window(id, path, pgid, geometry, m_windows.count(), format);
    m_windows.insert(path, window);
    m_windowMutex.unlock();
    sortWindows();
    connect(window, &Window::closed, this, [this, window, path]{
        O_INFO("Window" << window->identifier() << "closed");
        removeWindow(path);
        if(!DBusService::shuttingDown()){
            auto region = window->_geometry().intersected(m_screenGeometry.translated(-m_screenGeometry.topLeft()));
            m_thread.enqueue(nullptr, region, EPFrameBuffer::Initialize, 0, true);
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
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
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
    int marker = std::experimental::randint(1, 9999);
    Oxide::runInEventLoop([this, marker](std::function<void()> quit){
        m_thread.enqueue(nullptr, m_screenGeometry, EPFrameBuffer::HighQualityGrayscale, marker, true, quit);
    });
}

bool GuiAPI::isThisPgId(pid_t valid_pgid){
    if(!calledFromDBus()){
        return true;
    }
    auto pgid = getSenderPgid();
    return pgid == valid_pgid || pgid == getpid();
}

QList<Window*> GuiAPI::sortedWindows(){
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    auto sortedWindows = m_windows.values();
    std::sort(sortedWindows.begin(), sortedWindows.end());
    return sortedWindows;
}

void GuiAPI::sortWindows(){
    auto windows = sortedWindows();
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    int raisedZ = 0;
    int loweredZ = -1;
    for(auto window : windows){
        switch(window->state()){
            case Window::Raised:
            case Window::RaisedHidden:
                window->setZ(raisedZ++);
                break;
            case Window::Lowered:
            case Window::LoweredHidden:
                window->setZ(loweredZ--);
                break;
            case Window::Closed:
            default:
                break;
        }
    }
}

void GuiAPI::closeWindows(pid_t pgid){
    m_windowMutex.lock();
    QList<Window*> windows;
    for(auto window : windows){
        if(window->pgid() == pgid){
            windows.append(window);
        }
    }
    m_windowMutex.unlock();
    for(auto window : windows){
        window->_close();
    }
}

void GuiAPI::lowerWindows(pid_t pgid){
    m_windowMutex.lock();
    QList<Window*> windows;
    for(auto window : windows){
        if(window->pgid() == pgid){
            windows.append(window);
        }
    }
    m_windowMutex.unlock();
    for(auto window : windows){
        window->_lower();
    }
}

void GuiAPI::raiseWindows(pid_t pgid){
    m_windowMutex.lock();
    QList<Window*> windows;
    for(auto window : windows){
        if(window->pgid() == pgid){
            windows.append(window);
        }
    }
    m_windowMutex.unlock();
    for(auto window : windows){
        window->_raise();
    }
}

void GuiAPI::dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool async){
    Q_ASSERT(window != nullptr);
    if(async){
        m_thread.enqueue(window, region, waveform, marker);
        return;
    }
    marker = marker == 0 ? ++m_currentMarker : marker;
    O_DEBUG("Waiting for repaint" << marker);
    Oxide::runInEventLoop([this, window, region, waveform, marker](std::function<void()> quit){
        m_thread.enqueue(window, region, waveform, marker, false, [marker, quit]{
            O_DEBUG("Repaint callback done" << marker);
            quit();
        });
    });
    O_DEBUG("Repaint done" << marker);
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
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
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
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
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
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeEvent(args);
        }
    }
}

void GuiAPI::touchEvent(const input_event& event){
    O_EVENT(event);
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeTouchEvent(event);
        }
    }
}

void GuiAPI::tabletEvent(const input_event& event){
    O_EVENT(event);
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeTabletEvent(event);
        }
    }
}

void GuiAPI::keyEvent(const input_event& event){
    O_EVENT(event);
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    for(auto window : m_windows){
        if(window->_isVisible() && !window->isAppPaused()){
            window->writeKeyEvent(event);
        }
    }
}

bool GuiAPI::hasPermission(){
    if(DBusService::shuttingDown()){
        return false;
    }
    pid_t pgid = getSenderPgid();
    if(dbusService->isChildGroup(pgid)){
        return true;
    }
    return pgid == ::getpgrp();
}

void GuiAPI::removeWindow(QString path){
    QMutexLocker locker(&m_windowMutex);
    if(m_windows.remove(path)){
        locker.unlock();
        sortWindows();
    }
}
