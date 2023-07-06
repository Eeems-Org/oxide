#include "guiapi.h"
#include "appsapi.h"

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
    m_screenGeometry = deviceSettings.screenGeometry();
    Oxide::Sentry::sentry_transaction("gui", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "singleton", "Setup singleton", [this]{
            singleton(this);
        });
    });
}
GuiAPI::~GuiAPI(){

}
void GuiAPI::startup(){
    W_DEBUG("Startup");
}

QRect GuiAPI::geometry(){
    if(!hasPermission("gui")){
        W_DENIED();
        return QRect();
    }
    W_ALLOWED();
    return m_screenGeometry;
}

void GuiAPI::setEnabled(bool enabled){
    qDebug() << "GUI API" << enabled;
    m_enabled = enabled;
    for(auto window : windows){
        window->setEnabled(enabled);
    }
}

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
    auto window = new Window(id, path, pgid, geometry, this);
    windows.insert(path, window);
    connect(window, &Window::closed, [=]{
        windows.remove(path);
        window->deleteLater();
    });
    connect(window, &Window::dirty, [=](const QRect& region){
        auto geometry = window->geometry();
        const QRect intersection = region.intersected(geometry);
        const QPoint screenOffset = geometry.topLeft();
        setDirty(intersection.translated(-screenOffset));
    });
    for(auto item : appsAPI->runningApplicationsNoSecurityCheck().values()){
        Application* app = appsAPI->getApplication(item.value<QDBusObjectPath>());
        if(app->processId() == pgid){
            connect(app, &Application::paused, [=]{
                window->setVisible(false);
            });
            connect(app, &Application::resumed, [=]{
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

void GuiAPI::setDirty(const QRect& region){
    const QRect intersection = region.intersected(m_screenGeometry);
    const QPoint screenOffset = m_screenGeometry.topLeft();
    m_repaintRegion += intersection.translated(-screenOffset);
    scheduleUpdate();
}

void GuiAPI::redraw(){
    Oxide::dispatchToMainThread([this]{
        if(m_repaintRegion.isEmpty()){
            return;
        }
        const QPoint screenOffset = m_screenGeometry.topLeft();
        const QRect screenRect = m_screenGeometry.translated(-screenOffset);
        auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
        // TODO - wait until screen isn't busy
        QRegion repaintedRegion;
        QPainter painter(frameBuffer);
        Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::black;
        for(QRect rect : m_repaintRegion){
            rect = rect.intersected(screenRect);
            if(rect.isEmpty()){
                continue;
            }
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect, colour);
            // TODO - have some sort of stack to determine which window is on top
            for(auto window : windows){
                if(!window->isVisible()){
                    continue;
                }
                auto image = window->toImage();
                if(image == nullptr){
                    continue;
                }
                const QRect windowRect = window->geometry().translated(-screenOffset);
                const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
                painter.drawImage(rect, *image, windowIntersect);
                repaintedRegion += windowIntersect;
            }
        }
        painter.end();
        auto boundingRect = repaintedRegion.boundingRect();
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
        EPFrameBuffer::sendUpdate(boundingRect, waveform, mode, true);
        EPFrameBuffer::waitForLastUpdate();
        m_repaintRegion = QRegion();
    });
}

bool GuiAPI::isThisPgId(pid_t valid_pgid){
    if(!calledFromDBus()){
        return true;
    }
    auto pgid = getSenderPgid();
    return pgid == valid_pgid || pgid == getpid();
}

bool GuiAPI::event(QEvent* event){
    if(event->type() == QEvent::UpdateRequest){
        redraw();
        m_dirty = false;
        return true;
    }
    return QObject::event(event);
}

void GuiAPI::scheduleUpdate(){
    if(!m_dirty){
        m_dirty = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}
