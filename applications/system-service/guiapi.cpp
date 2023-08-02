#include "guiapi.h"
#include "appsapi.h"
#include "digitizerhandler.h"
#include "dbusservice.h"
#include "eventlistener.h"

#include <QScreen>
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
    O_DEBUG("Starting up GUI API");
    m_thread.m_screenGeometry = &m_screenGeometry;
    Oxide::startThreadWithPriority(&m_thread, QThread::TimeCriticalPriority);
}

void GuiAPI::shutdown(){
    O_DEBUG("Closing all remaining windows");
    while(!m_windows.isEmpty()){
        m_windows.first()->_close();
    }
    m_thread.shutdown();
    O_DEBUG("GUI API shutdown complete");
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

QByteArray GuiAPI::clipboard(){
    if(!hasClipboardPermission()){
        W_DENIED();
        return QByteArray();
    }
    W_ALLOWED();
    return m_clipboard;
}

void GuiAPI::setClipboard(QByteArray clipboard){
    if(!hasClipboardPermission()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    m_clipboard.swap(clipboard);
    emit clipboardChanged(m_clipboard);
}

void GuiAPI::setEnabled(bool enabled){
    O_DEBUG("GUI API" << enabled);
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

Window* GuiAPI::_createWindow(QString name, QRect geometry, QImage::Format format){
    auto id = QUuid::createUuid().toString(QUuid::Id128);
    auto path = QString(OXIDE_SERVICE_PATH) + "/window/" + QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
    auto pgid = getSenderPgid();
    m_windowMutex.lock();
    auto window = new Window(id, path, pgid, name, geometry, format);
    m_windows.insert(path, window);
    m_windowMutex.unlock();
    sortWindows();
    for(auto item : appsAPI->runningApplicationsNoSecurityCheck().values()){
        Application* app = appsAPI->getApplication(item.value<QDBusObjectPath>());
        if(app->processId() == pgid){
            O_DEBUG("Window is for application" << app->name().toStdString().c_str());
            connect(app, &Application::exited, window, [window]{ window->_close(); });
        }
    }
    window->setEnabled(m_enabled);
    return window;
}

QDBusObjectPath GuiAPI::createWindow(int x, int y, int width, int height, QString name, int format){
    if(!hasPermission()){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    Window* window = _createWindow(name, QRect(x, y, width, height), (QImage::Format)format);
    return window == nullptr ? QDBusObjectPath("/") : window->path();
}

QDBusObjectPath GuiAPI::createWindow(QRect geometry, QString name, int format){
    if(!hasPermission()){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    Window* window = _createWindow(name, geometry, (QImage::Format)format);
    return window == nullptr ? QDBusObjectPath("/") : window->path();
}

QDBusObjectPath GuiAPI::createWindow(QString name, int format){
    if(!hasPermission()){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    Window* window = _createWindow(name, m_screenGeometry, (QImage::Format)format);
    return window == nullptr ? QDBusObjectPath("/") : window->path();
}

QVector<QDBusObjectPath> GuiAPI::windows(){
    QVector<QDBusObjectPath> windows;
    if(!hasPermission()){
        W_DENIED();
        return windows;
    }
    W_ALLOWED();
    auto pgid = getSenderPgid();
    for(auto window : _sortedWindows()){
        if(window->pgid() == pgid){
            windows.append(window->path());
        }
    }
    return windows;
}

QVector<QDBusObjectPath> GuiAPI::visibleWindows(){
    QVector<QDBusObjectPath> windows;
    if(!hasPermission()){
        W_DENIED();
        return windows;
    }
    W_ALLOWED();
    auto pgid = getSenderPgid();
    for(auto window : _sortedVisibleWindows()){
        if(window->pgid() == pgid){
            windows.append(window->path());
        }
    }
    return windows;
}

QVector<QDBusObjectPath> GuiAPI::allWindows(){
    QVector<QDBusObjectPath> windows;
    if(!hasWindowPermission()){
        W_DENIED();
        return windows;
    }
    W_ALLOWED();
    for(auto window : _sortedWindows()){
        windows.append(window->path());
    }
    return windows;
}

QVector<QDBusObjectPath> GuiAPI::allVisibleWindows(){
    QVector<QDBusObjectPath> windows;
    if(!hasWindowPermission()){
        W_DENIED();
        return windows;
    }
    W_ALLOWED();
    for(auto window : _sortedVisibleWindows()){
        windows.append(window->path());
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

bool GuiAPI::hasWindowPermission(){
    if(!calledFromDBus() || qEnvironmentVariableIsSet("OXIDE_EXPOSE_ALL_WINDOWS")){
        return true;
    }
    auto pgid = getSenderPgid();
    if(pgid == getpid()){
        return true;
    }
    auto app = appsAPI->getApplication(pgid);
    return app != nullptr && app->permissions().contains("window");
}

bool GuiAPI::hasClipboardPermission(){
    if(!calledFromDBus()){
        return true;
    }
    auto pgid = getSenderPgid();
    if(pgid == getpid()){
        return true;
    }
    auto app = appsAPI->getApplication(pgid);
    return app == nullptr || app->permissions().contains("clipboard");
}

QVector<Window*> GuiAPI::_sortedWindows(){
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    auto sortedWindows = m_windows.values().toVector();
    std::sort(sortedWindows.begin(), sortedWindows.end(), [](const Window* w0, const Window* w1){ return w0->z() < w1->z(); });
    return sortedWindows;
}

QVector<Window*> GuiAPI::_sortedVisibleWindows(){
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    auto sortedWindows = m_windows.values().toVector();
    sortedWindows.erase(std::remove_if(sortedWindows.begin(), sortedWindows.end(), [](Window* w){ return !w->_isVisible(); }), sortedWindows.end());
    std::sort(sortedWindows.begin(), sortedWindows.end(), [](const Window* w0, const Window* w1){ return w0->z() < w1->z(); });
    return sortedWindows;
}

void GuiAPI::sortWindows(){
    auto windows = _sortedWindows();
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker)
    int raisedZ = 0;
    int loweredZ = -1;
    for(Window* window : windows){
        if(window->systemWindow()){
            continue;
        }
        auto pgid = window->pgid();
        if(pgid != ::getpid() && !dbusService->isChild(pgid) && !dbusService->isChildGroup(pgid)){
            O_DEBUG("Cleaning up window with no registered child:" << window->identifier().toStdString().c_str() << window->name().toStdString().c_str() << pgid);
            runLater(thread(), [window]{ window->_close(); });
            continue;
        }
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
    O_DEBUG("Closing windows for" << pgid);
    QVector<Window*> windows;
    for(auto window : m_windows.values()){
        if(window->pgid() == pgid){
            windows.append(window);
        }
    }
    m_windowMutex.unlock();
    std::sort(windows.begin(), windows.end(), [](const Window* w0, const Window* w1){ return w0->z() < w1->z(); });
    for(auto window : windows){
        O_DEBUG("Closing" << window->identifier() << window->name() << window->pgid());
        window->_close();
    }
}

void GuiAPI::lowerWindows(pid_t pgid){
    m_windowMutex.lock();
    O_DEBUG("Lowering windows for" << pgid);
    QVector<Window*> windows;
    for(auto window : m_windows.values()){
        if(window->pgid() == pgid){
            windows.append(window);
        }
    }
    m_windowMutex.unlock();
    std::sort(windows.begin(), windows.end(), [](const Window* w0, const Window* w1){ return w0->z() < w1->z(); });
    for(auto window : windows){
        O_DEBUG("Lowering" << window->identifier() << window->name() << window->pgid());
        window->_lower(false);
    }
}

void GuiAPI::raiseWindows(pid_t pgid){
    m_windowMutex.lock();
    O_DEBUG("Raising windows for" << pgid);
    QVector<Window*> windows;
    for(auto window : m_windows.values()){
        if(window->pgid() == pgid){
            windows.append(window);
        }
    }
    m_windowMutex.unlock();
    std::sort(windows.begin(), windows.end(), [](const Window* w0, const Window* w1){ return w0->z() < w1->z(); });
    for(auto window : windows){
        O_DEBUG("Raising" << window->identifier() << window->name() << window->pgid());
        window->_raise(false);
    }
}

bool GuiAPI::hasRaisedWindows(pid_t pgid){
    QMutexLocker locker(&m_windowMutex);
    Q_UNUSED(locker);
    for(auto window : m_windows.values()){
        if(window->pgid() == pgid && window->state() == Window::Raised){
            return true;
        }
    }
    return false;
}

void GuiAPI::dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool async){
    if(async){
        m_thread.enqueue(window, region, waveform, marker, window == nullptr);
        return;
    }
    marker = marker == 0 ? ++m_currentMarker : marker;
    O_DEBUG("Waiting for repaint" << marker);
    Oxide::runInEventLoop([this, window, region, waveform, marker](std::function<void()> quit){
        m_thread.enqueue(window, region, waveform, marker, window == nullptr, [marker, quit]{
            O_DEBUG("Repaint callback done" << marker);
            quit();
        });
    });
    O_DEBUG("Repaint done" << marker);
}

GUIThread* GuiAPI::guiThread(){ return &m_thread; }

void GuiAPI::writeTouchEvent(QEvent* event){
    W_DEBUG(event);
    auto windows = _sortedVisibleWindows();
    if(windows.isEmpty()){
        W_DEBUG("No visible windows");
        return;
    }
    auto window = windows.last();
    if(window->isAppPaused()){
        W_DEBUG("Application paused for window with focus");
        return;
    }
    W_DEBUG("Window with focus" << window->identifier() << window->name());
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
    bool valid = false;
    auto geometry = window->_geometry();
    auto touchEvent = static_cast<QTouchEvent*>(event);
    for(auto point : touchEvent->touchPoints()){
        if(geometry.contains(point.screenPos().toPoint())){
            valid = true;
        }
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
        auto pos = point.screenPos();
        args.points.push_back(TouchEventPoint{
            .id = point.id(),
            .state = state,
            .x = (pos.x() - geometry.x()) / qreal(geometry.width()),
            .y = (pos.y() - geometry.y()) / qreal(geometry.height()),
            .width = point.ellipseDiameters().width(),
            .height = point.ellipseDiameters().height(),
            .tool = TouchEventTool::Finger,
            .pressure = point.pressure(),
            .rotation = point.rotation(),
        });
        W_DEBUG(point.screenPos() << point.normalizedPos() << geometry);
    }
    if(valid){
        window->writeEvent(args);
    }else{
        args.type = TouchEventType::TouchCancel;
        window->writeEvent(args);
        // TODO - change window that has focus
    }
}

void GuiAPI::writeTabletEvent(QEvent* event){
    W_DEBUG(event);
    auto windows = _sortedVisibleWindows();
    if(windows.isEmpty()){
        W_DEBUG("No visible windows");
        return;
    }
    auto window = windows.last();
    if(window->isAppPaused()){
        W_DEBUG("Application paused for window with focus");
        return;
    }
    W_DEBUG("Window with focus" << window->identifier() << window->name());
    auto tabletEvent = static_cast<QTabletEvent*>(event);
    auto pos = tabletEvent->globalPos();
    auto geometry = window->_geometry();
    if(!geometry.contains(pos)){
        // TODO - cancel tablet events
        // TODO - change window that has focus
        return;
    }
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
            type = TabletEventType::PenEnterProximity;
            break;
        case QEvent::TabletLeaveProximity:
            type = TabletEventType::PenLeaveProximity;
            break;
        default:
            return;
    }
    TabletEventTool tool = TabletEventTool::Pen;
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
    // Translate position to position on window
    TabletEventArgs args{
        .type = type,
        .tool = tool,
        .x = pos.x() - geometry.x(),
        .y = pos.y() - geometry.y(),
        .pressure = (double)tabletEvent->pressure(),
        .tiltX = tabletEvent->xTilt(),
        .tiltY = tabletEvent->yTilt(),
    };
    window->writeEvent(args);
}

void GuiAPI::writeKeyEvent(QEvent* event){
    W_DEBUG(event);
    auto windows = _sortedVisibleWindows();
    if(windows.isEmpty()){
        W_DEBUG("No visible windows");
        return;
    }
    auto window = windows.last();
    if(window->isAppPaused()){
        W_DEBUG("Application paused for window with focus");
        return;
    }
    W_DEBUG("Window with focus" << window->identifier() << window->name());
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
        .scanCode = (unsigned int)keyEvent->nativeScanCode(),
    };
    window->writeEvent(args);
}

bool GuiAPI::hasPermission(){
    if(DBusService::shuttingDown()){
        return false;
    }
    pid_t pgid = getSenderPgid();
    if(dbusService->isChildGroup(pgid) || pgid == ::getpgrp()){
        return true;
    }
    return hasWindowPermission();
}

void GuiAPI::removeWindow(QString path){
    QMutexLocker locker(&m_windowMutex);
    if(m_windows.remove(path)){
        O_DEBUG("Window" << path << "removed from list");
        locker.unlock();
        sortWindows();
    }
}
