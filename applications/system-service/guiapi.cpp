#include "guiapi.h"
#include "appsapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"
#include "dbusservice.h"

#include <QSignalSpy>

#define W_WARNING(msg) O_WARNING(__PRETTY_FUNCTION__ << msg << getSenderPgid())
#define W_DEBUG(msg) O_DEBUG(__PRETTY_FUNCTION__ << msg << getSenderPgid())
#define W_DENIED() W_DEBUG("DENY")
#define W_ALLOWED() W_DEBUG("ALLOW")

static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));

static GUIThread m_thread;

GuiAPI* GuiAPI::singleton(GuiAPI* self){
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
    m_thread.setObjectName("gui");
    m_thread.m_repaintNotifier = &m_repaintNotifier;
    m_thread.m_screenGeometry = &m_screenGeometry;
    m_thread.start();
    m_thread.moveToThread(&m_thread);
}

void GuiAPI::shutdown(){
    W_DEBUG("Shutdown");
    while(!m_windows.isEmpty()){
        auto window = m_windows.take(m_windows.firstKey());
        window->_close();
    }
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
        auto region = window->_geometry().intersected(m_screenGeometry.translated(-m_screenGeometry.topLeft()));
        window->setEnabled(false);
        window->deleteLater();
        QCoreApplication::postEvent(&m_thread, new RepaintEvent(nullptr, region, EPFrameBuffer::Initialize), Qt::HighEventPriority);
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
    QCoreApplication::postEvent(&m_thread, new RepaintEvent(nullptr, m_screenGeometry, EPFrameBuffer::HighQualityGrayscale), Qt::HighEventPriority);
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
    // TODO - identify if there is actually a pending paint
    QSignalSpy spy(&m_repaintNotifier, &RepaintNotifier::repainted);
    // TODO - determine if there is a reasonable max time to wait
    spy.wait();
}

void GuiAPI::dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform){
    QCoreApplication::postEvent(&m_thread, new RepaintEvent(window, region, waveform), Qt::HighEventPriority);
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
