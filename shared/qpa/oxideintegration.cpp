#include "oxideintegration.h"
#include "oxidebackingstore.h"
#include "oxidescreen.h"
#include "oxidetabletdata.h"
#include "oxidetouchscreendata.h"
#include "oxideeventfilter.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <qpa/qplatformfontdatabase.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatformwindow.h>

#include <private/qevdevmousehandler_p.h>
#include <private/qevdevtouchhandler_p.h>
#include <private/qevdevtablethandler_p.h>
#include <private/qevdevkeyboardhandler_p.h>
#include <private/qgenericunixeventdispatcher_p.h>
#include <private/qgenericunixfontdatabase_p.h>
#include <liboxide.h>
#include <liboxide/tarnish.h>

QT_BEGIN_NAMESPACE

class QCoreTextFontEngine;

static const char debugQPAEnvironmentVariable[] = "QT_DEBUG_OXIDE_QPA";


static inline unsigned parseOptions(const QStringList& paramList){
    unsigned options = 0;
    for (const QString &param : paramList) {
        if(!param.compare(QLatin1String("enable_fonts"), Qt::CaseInsensitive)){
            options |= OxideIntegration::EnableFonts;
        }else if(!param.compare(QLatin1String("freetype"), Qt::CaseInsensitive)){
            options |= OxideIntegration::FreeTypeFontDatabase;
        }else if(!param.compare(QLatin1String("fontconfig"), Qt::CaseInsensitive)){
            options |= OxideIntegration::FontconfigDatabase;
        }
    }
    QWindowSystemInterfacePrivate::TabletEvent::setPlatformSynthesizesMouse(true);
    return options;
}

OxideIntegration::OxideIntegration(const QStringList& parameters): m_fontDatabase(nullptr), m_options(parseOptions(parameters)), m_debug(false), m_spec(parameters){
    if(
        qEnvironmentVariableIsSet(debugQPAEnvironmentVariable)
        && qEnvironmentVariableIntValue(debugQPAEnvironmentVariable) > 0
    ) {
        m_options |= DebugQPA | EnableFonts;
        m_debug = true;
    }

    m_primaryScreen = new OxideScreen();
    QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
}

OxideIntegration::~OxideIntegration(){
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
    delete m_fontDatabase;
}

bool OxideIntegration::hasCapability(QPlatformIntegration::Capability cap) const{
    if(m_debug){
        qDebug() << "OxideIntegration::hasCapability";
    }
    switch (cap) {
        case ThreadedPixmaps: return true;
        case MultipleWindows: return true;
        default: return QPlatformIntegration::hasCapability(cap);
    }
}

void OxideIntegration::initialize(){
    if(m_debug){
        qDebug() << "OxideIntegration::initialize";
    }
    QWindowSystemInterfacePrivate::TabletEvent::setPlatformSynthesizesMouse(true);
    auto socket = Oxide::Tarnish::getSocket();
    if(socket == nullptr){
        qFatal("Could not get tarnish private socket");
    }
    QObject::connect(Oxide::Tarnish::getSocket(), &QLocalSocket::readChannelFinished, []{ qApp->quit(); });
    QObject::connect(socket, &QLocalSocket::readyRead, [socket]{
        // TODO - read commands
        while(!socket->atEnd()){
            O_DEBUG(socket->readAll());
        }
    });
    qApp->installEventFilter(new OxideEventFilter(qApp));
    m_inputContext = QPlatformInputContextFactory::create();
    // Setup touch event handling
    auto touchData = new OxideTouchScreenData(m_spec);
    auto touchPipe = Oxide::Tarnish::getTouchEventPipe();
    if(touchPipe == nullptr){
        qFatal("Could not get touch event pipe");
    }
    QObject::connect(touchPipe, &QLocalSocket::readChannelFinished, []{
        if(!qApp->closingDown()){
            qApp->exit(EXIT_FAILURE);
        }
    });
    QObject::connect(touchPipe, &Oxide::Tarnish::InputEventSocket::inputEvent, [touchData](input_event event){
        O_EVENT(event);
        touchData->processInputEvent(&event);
    });
    // Setup tablet event handling
    auto tabletData = new OxideTabletData(Oxide::Tarnish::getTabletEventPipeFd());
    auto tabletPipe = Oxide::Tarnish::getTouchEventPipe();
    QObject::connect(tabletPipe, &QLocalSocket::readChannelFinished, []{
        if(!qApp->closingDown()){
            qApp->exit(EXIT_FAILURE);
        }
    });
    if(tabletPipe == nullptr){
        qFatal("Could not get tablet event pipe");
    }
    connect(tabletPipe, &Oxide::Tarnish::InputEventSocket::inputEvent, [tabletData](input_event event){
        O_EVENT(event);
        tabletData->processInputEvent(&event);
    });
    // Setup key event handling
    QFdContainer keyHandlerFd(Oxide::Tarnish::getKeyEventPipeFd());
    auto keyHandler = new QEvdevKeyboardHandler("oxide", keyHandlerFd, false, false, "");
    auto eventPipe = Oxide::Tarnish::getEventPipe();
    if(eventPipe == nullptr){
        qFatal("Could not get event pipe");
    }
    QObject::connect(eventPipe, &QLocalSocket::readChannelFinished, []{
        if(!qApp->closingDown()){
            qApp->exit(EXIT_FAILURE);
        }
    });
    QObject::connect(eventPipe, &QLocalSocket::readyRead, eventPipe, [this, eventPipe, keyHandler]{
        QMutexLocker locker(&m_mutex);
        Q_UNUSED(locker);
        while(!eventPipe->atEnd()){
            auto event = Oxide::Tarnish::WindowEvent::fromSocket(eventPipe);
            switch(event.type){
                case Oxide::Tarnish::Geometry:{
                    m_primaryScreen->setGeometry(event.geometryData.geometry);
                    auto window = m_primaryScreen->topWindow();
                    if(window != nullptr){
                        window->setGeometry(event.geometryData.geometry);
                    }
                    break;
                }
                case Oxide::Tarnish::Raise:{
                    auto window = m_primaryScreen->topWindow();
                    if(window != nullptr){
                        window->raise();
                    }
                    break;
                }
                case Oxide::Tarnish::Lower:{
                    auto window = m_primaryScreen->topWindow();
                    if(window != nullptr){
                        window->lower();
                    }
                    break;
                }
                case Oxide::Tarnish::Close:{
                    auto window = m_primaryScreen->topWindow();
                    if(window != nullptr){
                        window->close();
                    }
                    delete keyHandler;
                    break;
                }
                case Oxide::Tarnish::Ping:
                    event.toSocket(eventPipe);
                    break;
                case Oxide::Tarnish::WaitForPaint:
                    QMetaObject::invokeMethod(
                        m_primaryScreen,
                        "waitForPaint",
                        Qt::QueuedConnection,
                        Q_ARG(unsigned int, event.waitForPaintData.marker)
                    );
                    break;
                case Oxide::Tarnish::Key:{
                    auto data = event.keyData;
                    QEvent::Type type = QEvent::None;
                    bool repeat = false;
                    switch(data.type){
                        case Oxide::Tarnish::Release:
                            type = QEvent::KeyRelease;
                            break;
                        case Oxide::Tarnish::Press:
                            type = QEvent::KeyPress;
                            break;
                        case Oxide::Tarnish::Repeat:
                            type = QEvent::KeyPress;
                            repeat = true;
                            break;
                    }
                    // TODO - sort out how to get key modifiers state
                    QWindowSystemInterface::handleKeyEvent(
                        nullptr,
                        type,
                        data.code,
                        Qt::NoModifier,
                        "",
                        repeat
                    );
                    break;
                }
                case Oxide::Tarnish::Touch:{
                    auto data = event.touchData;
                    // TODO - emit touch event
                    break;
                }
                case Oxide::Tarnish::ImageInfo:
                case Oxide::Tarnish::Repaint:
                case Oxide::Tarnish::FrameBuffer:
                case Oxide::Tarnish::Invalid:
                default:
                    break;
            }
        }
    }, Qt::QueuedConnection);
}


// Dummy font database that does not scan the fonts directory to be
// used for command line tools like qmlplugindump that do not create windows
// unless DebugBackingStore is activated.
class DummyFontDatabase : public QPlatformFontDatabase{
public:
    void populateFontDatabase() override {}
};

QPlatformFontDatabase* OxideIntegration::fontDatabase() const{
    if(m_debug){
        qDebug() << "OxideIntegration::fontDatabase";
    }
    if(!m_fontDatabase && (m_options & EnableFonts)){
        if(!m_fontDatabase){
#if QT_CONFIG(fontconfig)
            m_fontDatabase = new QGenericUnixFontDatabase;
#else
            m_fontDatabase = QPlatformIntegration::fontDatabase();
#endif
        }
    }
    if(!m_fontDatabase){
        m_fontDatabase = new DummyFontDatabase;
    }
    return m_fontDatabase;
}

QPlatformInputContext* OxideIntegration::inputContext() const{
    if(m_debug){
        qDebug() << "OxideIntegration::inputContext";
    }
    return m_inputContext;
}

QPlatformWindow* OxideIntegration::createPlatformWindow(QWindow *window) const{
    if(m_debug){
        qDebug() << "OxideIntegration::createPlatformWindow";
    }
    OxideWindow* w = new OxideWindow(window);
    w->requestActivateWindow();
    return w;
}

QPlatformBackingStore* OxideIntegration::createPlatformBackingStore(QWindow *window) const{
    if(m_debug){
        qDebug() << "OxideIntegration::createPlatformBackingStore";
    }
    return new OxideBackingStore(window);
}

QAbstractEventDispatcher* OxideIntegration::createEventDispatcher() const{
    if(m_debug){
        qDebug() << "OxideIntegration::createEventDispatcher";
    }
    return createUnixEventDispatcher();
}

QPlatformNativeInterface* OxideIntegration::nativeInterface() const{
    if(m_debug){
        qDebug() << "OxideIntegration::nativeInterface";
    }
    return const_cast<OxideIntegration*>(this);
}

OxideIntegration* OxideIntegration::instance(){
    auto instance = static_cast<OxideIntegration*>(QGuiApplicationPrivate::platformIntegration());
    if(instance->m_debug){
        qDebug() << "OxideIntegration::instance";
    }
    return instance;
}

QT_END_NAMESPACE
