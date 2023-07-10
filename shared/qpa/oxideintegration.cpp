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
    QObject::connect(Oxide::Tarnish::getSocket(), &QLocalSocket::readChannelFinished, []{ qApp->exit(EXIT_FAILURE); });
    connect(socket, &QLocalSocket::readyRead, [socket]{
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
    QObject::connect(touchPipe, &QLocalSocket::readChannelFinished, []{ qApp->exit(EXIT_FAILURE); });
    connect(touchPipe, &Oxide::Tarnish::InputEventSocket::inputEvent, [touchData](input_event event){
        O_EVENT(event);
        touchData->processInputEvent(&event);
    });
    // Setup tablet event handling
    auto tabletData = new OxideTabletData(Oxide::Tarnish::getTabletEventPipeFd());
    auto tabletPipe = Oxide::Tarnish::getTouchEventPipe();
    QObject::connect(tabletPipe, &QLocalSocket::readChannelFinished, []{ qApp->exit(EXIT_FAILURE); });
    if(tabletPipe == nullptr){
        qFatal("Could not get tablet event pipe");
    }
    connect(tabletPipe, &Oxide::Tarnish::InputEventSocket::inputEvent, [tabletData](input_event event){
        O_EVENT(event);
        tabletData->processInputEvent(&event);
    });
    // Setup key event handling
    auto keyPipeFd = QFdContainer(Oxide::Tarnish::getKeyEventPipeFd());
    auto keyHandler = new QEvdevKeyboardHandler("oxide", keyPipeFd, false, false, "");
    QObject::connect(Oxide::Tarnish::topWindow(), &codes::eeems::oxide1::Window::closed, [=]{ delete keyHandler; });
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
