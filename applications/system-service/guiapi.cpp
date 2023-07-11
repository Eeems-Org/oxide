#include "guiapi.h"
#include "appsapi.h"
#include "digitizerhandler.h"
#include "dbusservice.h"

#include <QSignalSpy>

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
        window->_close();
    }
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
    if(!hasPermission()){
        W_DENIED();
        return nullptr;
    }
    W_ALLOWED();
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
        QMutableListIterator i(m_repaintList);
        while(i.hasNext()){
            Repaint& item = i.next();
            if(item.window == window){
                i.remove();
            }
        }
        auto region = window->_geometry().intersected(m_screenGeometry.translated(-m_screenGeometry.topLeft()));
        m_repaintList.append(Repaint{
            .window = nullptr,
            .region = region,
            .waveform = EPFrameBuffer::Initialize
        });
        window->setEnabled(false);
        window->deleteLater();
        scheduleUpdate();
    });
    connect(window, &Window::dirty, this, [=](const QRect& region, EPFrameBuffer::WaveformMode waveform){
        m_repaintList.append(Repaint{
            .window = window,
            .region = region,
            .waveform = waveform
        });
        scheduleUpdate();
    });
    connect(window, &Window::raised, this, [=]{
        auto windows = sortedWindows();
        int z = 0;
        for(auto w : windows){
            if(w == window){
                w->setZ(z++);
            }
        }
        window->setZ(z);
    });
    connect(window, &Window::lowered, this, [=]{
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
    Window* window = _createWindow(
        QRect(
            m_screenGeometry.x(),
            m_screenGeometry.y(),
            m_screenGeometry.width(),
            m_screenGeometry.height()
        ),
        (QImage::Format)format
    );
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
    // Get the regions that need to be repainted
    const QPoint screenOffset = m_screenGeometry.topLeft();
    const QRect screenRect = m_screenGeometry.translated(-screenOffset);
    QRegion repaintRegion;
    auto waveform = EPFrameBuffer::Initialize;
    while(!m_repaintList.isEmpty()){
        auto item = m_repaintList.takeFirst();
        auto window = item.window;
        auto region = item.region;
        if(region.isEmpty()){
            continue;
        }
        QRect rect;
        if(window == nullptr){
            rect = region;
        }else{
            // Get visible region on the screen to repaint
            auto geometry = window->_geometry();
            const QRect intersection = region.intersected(geometry);
            const QPoint screenOffset = geometry.topLeft();
            rect = intersection.translated(-screenOffset).intersected(screenRect);
        }
        if(rect.isEmpty()){
            continue;
        }
        repaintRegion += rect;
        // TODO - have way to do per-region waveform updates instead of just grouping them all with the largest
        if(item.waveform > waveform){
            waveform = item.waveform;
        }
    }
    // Get windows in order of Z sort order, and filter out invalid windows
    auto visibleWindows = sortedWindows();
    QMutableListIterator i(visibleWindows);
    while(i.hasNext()){
        auto window = i.next();
        if(window == nullptr){
            i.remove();
            continue;
        }
        if(!window->_isVisible()){
            i.remove();
            continue;
        }
        auto image = window->toImage();
        if(image.isNull()){
            O_WARNING(__PRETTY_FUNCTION__ << window->identifier() << "Null framebuffer");
            i.remove();
            continue;
        }
        window->lock();
    }
    // Paint the regions
    // TODO - explore using QPainter::clipRegion to see if it can speed things up
    QRegion repaintedRegion;
    auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::white;
    QPainter painter(frameBuffer);
    for(QRect rect : repaintRegion){
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, colour);
        for(auto window : visibleWindows){
            const QRect windowRect = window->_geometry().translated(-screenOffset);
            const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
            O_DEBUG(__PRETTY_FUNCTION__ << window->identifier() << rect << windowIntersect);
            // TODO - See if there is a way to detect if there is just transparency in the region
            //        and don't mark this as repainted.
            painter.drawImage(rect, window->toImage(), windowIntersect);
            repaintedRegion += windowIntersect;
        }
    }
    painter.end();
    // Send updates for all the repainted regions
    for(auto rect : repaintedRegion){
        // TODO - detect if there was no change to the repainted region and skip, maybe compare against previous window states?
        //        Maybe hash the data before and compare after? https://doc.qt.io/qt-5/qcryptographichash.html
        if(waveform == EPFrameBuffer::Initialize){
            waveform = EPFrameBuffer::Mono;
            // TODO - profile if it makes sense to do this instead of just picking one to always use
            for(int x = rect.left(); x < rect.right(); x++){
                for(int y = rect.top(); y < rect.bottom(); y++){
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
        }
        auto mode =  rect == screenRect ? EPFrameBuffer::FullUpdate : EPFrameBuffer::PartialUpdate;
        EPFrameBuffer::sendUpdate(rect, waveform, mode);
    }
    EPFrameBuffer::waitForLastUpdate();
    for(auto window : visibleWindows){
        window->unlock();
    }
    m_dirty = false;
    emit m_repaintNotifier.repainted();
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
    if(m_dirty){
        QSignalSpy spy(&m_repaintNotifier, &RepaintNotifier::repainted);
        // TODO - determine if there is a reasonable max time to wait
        spy.wait();
    }
}

void GuiAPI::touchEvent(const input_event& event){
    O_EVENT(event);
    for(auto window : m_windows){
        if(window != nullptr && window->_isVisible()){
            window->writeTouchEvent(event);
        }
    }
}

void GuiAPI::tabletEvent(const input_event& event){
    O_EVENT(event);
    for(auto window : m_windows){
        if(window != nullptr && window->_isVisible()){
            window->writeTabletEvent(event);
        }
    }
}

void GuiAPI::keyEvent(const input_event& event){
    O_EVENT(event);
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
