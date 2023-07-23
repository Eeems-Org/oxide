#include "oxideintegration.h"
#include "oxidebackingstore.h"
#include "oxidescreen.h"
#include "oxideeventfilter.h"
#include "oxideeventhandler.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <qpa/qplatformfontdatabase.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatformwindow.h>

#include <private/qgenericunixeventdispatcher_p.h>
#include <private/qgenericunixfontdatabase_p.h>
#include <liboxide.h>

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

OxideIntegration::OxideIntegration(const QStringList& parameters)
: m_fontDatabase(nullptr),
  m_options(parseOptions(parameters)),
  m_debug(false),
  m_spec(parameters)
{
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
    QObject::connect(Oxide::Tarnish::getSocket(), &QLocalSocket::disconnected, []{ qApp->quit(); });
    QObject::connect(socket, &QLocalSocket::readyRead, [socket]{
        // TODO - read commands
        while(!socket->atEnd()){
            O_DEBUG(socket->readAll());
        }
    });
    qApp->installEventFilter(new OxideEventFilter(qApp));
    m_inputContext = QPlatformInputContextFactory::create();
    auto eventPipe = Oxide::Tarnish::getEventPipe();
    if(eventPipe == nullptr){
        qFatal("Could not get event pipe");
    }
    QObject::connect(eventPipe, &QLocalSocket::disconnected, []{
        if(!qApp->closingDown()){
            qApp->exit(EXIT_FAILURE);
        }
    });
    new OxideEventHandler(eventPipe, m_primaryScreen);
    connectSignal(signalHandler, "sigCont()", m_primaryScreen, "raiseTopWindow()");
    connectSignal(signalHandler, "sigUsr1()", m_primaryScreen, "raiseTopWindow()");
    connectSignal(signalHandler, "sigUsr2()", m_primaryScreen, "lowerTopWindow()");
    connectSignal(signalHandler, "sigTerm()", m_primaryScreen, "closeTopWindow()");
    connectSignal(signalHandler, "sigInt()", m_primaryScreen, "closeTopWindow()");
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

void OxideIntegration::connectSignal(QObject* sender, QString signal, QObject* reciever, QString slot){
    auto signalIndex = sender->metaObject()->indexOfSignal(signal.toStdString().c_str());
    if(signalIndex == -1){
        qFatal(QString("The index of the signal:").arg(signal).toStdString().c_str());
    }
    auto signalMethod = sender->metaObject()->method(signalIndex);
    if(!signalMethod.isValid()){
        qFatal(QString("Cannot find signal from index: %1").arg(signal).toStdString().c_str());
    }
    auto slotIndex = reciever->metaObject()->indexOfSlot(slot.toStdString().c_str());
    if(slotIndex == -1){
        qFatal(QString("The index of the slot: %1").arg(slot).toStdString().c_str());
    }
    auto slotMethod = reciever->metaObject()->method(slotIndex);
    if(!slotMethod.isValid()){
        qFatal(QString("Cannot find slot from index: %1").arg(slot).toStdString().c_str());
    }
    QObject::connect(sender, signalMethod, reciever, slotMethod);
}

QT_END_NAMESPACE
