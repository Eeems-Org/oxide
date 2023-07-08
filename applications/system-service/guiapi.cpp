#include "guiapi.h"
#include "appsapi.h"
#include "digitizerhandler.h"

#define W_WARNING(msg) O_WARNING(__PRETTY_FUNCTION__ << msg << getSenderPgid())
#define W_DEBUG(msg) O_DEBUG(__PRETTY_FUNCTION__ << msg << getSenderPgid())
#define W_DENIED() W_DEBUG("DENY")
#define W_ALLOWED() W_DEBUG("ALLOW")

static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));

GuiAPI* GuiAPI::singleton(GuiAPI* self){
    static GuiAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}
GuiAPI::GuiAPI(QObject* parent)
: APIBase(parent),
  m_enabled(false),
  m_dirty(false)
{
    Oxide::Sentry::sentry_transaction("gui", "init", [this](Oxide::Sentry::Transaction* t){
        Q_UNUSED(t);
        m_screenGeometry = deviceSettings.screenGeometry();
        singleton(this);
        connect(touchHandler, &DigitizerHandler::inputEvent, this, &GuiAPI::touchEvent);
        connect(wacomHandler, &DigitizerHandler::inputEvent, this, &GuiAPI::touchEvent);
        connect(buttonHandler, &ButtonHandler::rawEvent, this, &GuiAPI::touchEvent);
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
}

void GuiAPI::shutdown(){
    W_DEBUG("Shutdown");
    while(!m_windows.isEmpty()){
        auto window = m_windows.take(m_windows.firstKey());
        window->close();
    }
}

QRect GuiAPI::geometry(){
    if(!hasPermission("gui")){
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

QDBusObjectPath GuiAPI::createWindow(int x, int y, int width, int height){ return createWindow(QRect(x, y, width, height)); }

QDBusObjectPath GuiAPI::createWindow(QRect geometry){
    if(!hasPermission("gui")){
        W_DENIED();
        return QDBusObjectPath("/");
    }
    W_ALLOWED();
    auto id = QUuid::createUuid().toString(QUuid::Id128);
    auto path = QString(OXIDE_SERVICE_PATH) + "/window/" + QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
    auto pgid = getSenderPgid();
    auto window = new Window(id, path, pgid, geometry);
    m_windows.insert(path, window);
    sortWindows();
    connect(window, &Window::closed, this, [this, window, path]{
        if(m_windows.remove(path)){
            sortWindows();
        }
        window->setEnabled(false);
        window->deleteLater();
    });
    connect(window, &Window::destroyed, this, [this, path]{
        if(m_windows.remove(path)){
            sortWindows();
        }
    });
    connect(window, &Window::dirty, this, [=](const QRect& region){
        m_repaintList.append(Repaint{
            .window = window,
            .region = region
        });
        scheduleUpdate();
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
    return window->path();
}

QDBusObjectPath GuiAPI::createWindow(){
    return createWindow(
        m_screenGeometry.x(),
        m_screenGeometry.y(),
        m_screenGeometry.width(),
        m_screenGeometry.height()
    );
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

void GuiAPI::redraw(){
    // This should already be on the main thread, but just in case
    if(qApp->thread() != QThread::currentThread()){
        O_WARNING(__PRETTY_FUNCTION__ << "Not called from main thread");
        return;
    }
    if(!m_dirty){
        O_WARNING(__PRETTY_FUNCTION__ << "Not dirty");
        return;
    }
    if(m_repaintList.isEmpty()){
        O_WARNING(__PRETTY_FUNCTION__ << "Nothing to repaint");
        m_dirty = false;
        return;
    }
    O_WARNING(__PRETTY_FUNCTION__ << "Repainting");
    const QPoint screenOffset = m_screenGeometry.topLeft();
    const QRect screenRect = m_screenGeometry.translated(-screenOffset);
    QMap<QString, QRegion> repaintList;
    while(!m_repaintList.isEmpty()){
        auto item = m_repaintList.takeFirst();
        auto window = item.window;
        auto region = item.region;
        auto geometry = window->_geometry();
        const QRect intersection = region.intersected(geometry);
        const QPoint screenOffset = geometry.topLeft();
        auto rect = intersection.translated(-screenOffset).intersected(screenRect);
        if(rect.isEmpty()){
            continue;
        }
        if(!repaintList.contains(window->identifier())){
            repaintList.insert(window->identifier(), QRegion());
        }
        QRegion repaintRegion = repaintList.value(window->identifier());
        repaintRegion += rect;
        repaintList.insert(window->identifier(), repaintRegion);
    }
    QRegion repaintedRegion;
    auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::black;
    QPainter painter(frameBuffer);
    for(auto window : m_windows){
        if(!repaintList.contains(window->identifier())){
            continue;
        }
        for(QRect rect : repaintList.value(window->identifier())){
            if(rect.isEmpty()){
                continue;
            }
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect, colour);
            // TODO - have some sort of stack to determine which window is on top
            for(auto window : m_windows){
                if(window == nullptr){
                    continue;
                }
                if(!window->_isVisible()){
                    O_WARNING(__PRETTY_FUNCTION__ << window->identifier() << "Not visible");
                    continue;
                }
                auto image = window->toImage();
                if(image.isNull()){
                    O_WARNING(__PRETTY_FUNCTION__ << window->identifier() << "Null framebuffer");
                    continue;
                }
                const QRect windowRect = window->geometry().translated(-screenOffset);
                const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
                O_WARNING(__PRETTY_FUNCTION__ << window->identifier() << rect << windowIntersect);
                painter.drawImage(rect, image, windowIntersect);
                repaintedRegion += windowIntersect;
            }
        }
    }
    painter.end();
    auto boundingRect = repaintedRegion.boundingRect();
    // TODO - profile if it makes sense to do this instead of just picking one to always use
    auto waveform = EPFrameBuffer::Mono;
    for(int x = boundingRect.left(); x < boundingRect.right(); x++){
        for(int y = boundingRect.top(); y < boundingRect.bottom(); y++){
            auto color = frameBuffer->pixelColor(x, y);
            if(color == Qt::white || color == Qt::black || color == Qt::transparent){
                continue;
            }
            if(color == Qt::gray){
                waveform = EPFrameBuffer::Grayscale;
                continue;
            }
            waveform = EPFrameBuffer::HighQualityGrayscale;
            break;
        }
    }
    auto mode =  boundingRect == screenRect ? EPFrameBuffer::FullUpdate : EPFrameBuffer::PartialUpdate;
    EPFrameBuffer::sendUpdate(boundingRect, waveform, mode);
    m_dirty = false;
}

bool GuiAPI::isThisPgId(pid_t valid_pgid){
    if(!calledFromDBus()){
        return true;
    }
    auto pgid = getSenderPgid();
    return pgid == valid_pgid || pgid == getpid();
}

QMap<QString, Window*> GuiAPI::allWindows(){ return m_windows; }

void GuiAPI::closeWindows(pid_t pgid){
    for(auto window : m_windows.values()){
        if(window->pgid() == pgid){
            window->close();
        }
    }
}

void GuiAPI::touchEvent(const input_event& event){
#ifdef DEBUG_EVENTS
    qDebug() << __PRETTY_FUNCTION__ << event.time.tv_sec << event.time.tv_usec << event.type << event.code << event.value;
#endif
    for(auto window : m_windows){
        if(window != nullptr && window->_isVisible()){
            window->writeTouchEvent(event);
        }
    }
}
void GuiAPI::tabletEvent(const input_event& event){
#ifdef DEBUG_EVENTS
    qDebug() << __PRETTY_FUNCTION__ << event.time.tv_sec << event.time.tv_usec << event.type << event.code << event.value;
#endif
    for(auto window : m_windows){
        if(window != nullptr && window->_isVisible()){
            window->writeTabletEvent(event);
        }
    }
}
void GuiAPI::keyEvent(const input_event& event){
#ifdef DEBUG_EVENTS
    qDebug() << __PRETTY_FUNCTION__ << event.time.tv_sec << event.time.tv_usec << event.type << event.code << event.value;
#endif
    for(auto window : m_windows){
        if(window != nullptr && window->_isVisible()){
            window->writeKeyEvent(event);
        }
    }
}

bool GuiAPI::event(QEvent* event){
    if(event->type() != QEvent::UpdateRequest){
        return QObject::event(event);
    }
    QTimer* timer = new QTimer(this);
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, this, [timer, this](){
        redraw();
        timer->deleteLater();
    }, Qt::QueuedConnection);
    timer->start(0);
    return true;
}

void GuiAPI::scheduleUpdate(){
    if(!m_dirty){
        m_dirty = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void GuiAPI::sortWindows(){
    int z = 0;
    for(auto path : m_windows.keys()){
        auto window = m_windows.value(path);
        if(window == nullptr){
            m_windows.remove(path);
        }
        window->setZ(z++);
    }
}
