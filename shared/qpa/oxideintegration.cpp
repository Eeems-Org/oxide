#include "oxideintegration.h"
#include "oxidebackingstore.h"
#include "oxidescreen.h"
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

#define debugQPAEnvironmentVariable "QT_DEBUG_OXIDE_QPA"


static inline unsigned short parseOptions(const QStringList& paramList){
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
  m_debug(false)
{
    if(m_debug){
        qDebug() << "OxideIntegration::OxideIntegration";
    }
    if(
        qEnvironmentVariableIsSet(debugQPAEnvironmentVariable)
        && qEnvironmentVariableIntValue(debugQPAEnvironmentVariable) > 0
    ){
        m_options |= DebugQPA | EnableFonts;
        m_debug = true;
    }
}

OxideIntegration::~OxideIntegration(){
    if(m_debug){
        qDebug() << "OxideIntegration::~OxideIntegration";
    }
}

bool OxideIntegration::hasCapability(QPlatformIntegration::Capability cap) const{
    if(m_debug){
        qDebug() << "OxideIntegration::hasCapability";
    }
    switch(cap){
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
    m_primaryScreen = new OxideScreen();
    QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
    auto socket = Oxide::Tarnish::getSocket();
    if(socket == nullptr){
        qFatal("Could not get tarnish private socket");
    }
    QObject::connect(Oxide::Tarnish::getSocket(), &QLocalSocket::disconnected, []{
        if(!qApp->closingDown()){
            qApp->quit();
        }
    });
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
    new OxideEventHandler(eventPipe);
    connectSignal(signalHandler, "sigCont()", m_primaryScreen, "raiseTopWindow()");
    connectSignal(signalHandler, "sigUsr1()", m_primaryScreen, "raiseTopWindow()");
    connectSignal(signalHandler, "sigUsr2()", m_primaryScreen, "lowerTopWindow()");
    connectSignal(signalHandler, "sigTerm()", m_primaryScreen, "closeTopWindow()");
    connectSignal(signalHandler, "sigInt()", m_primaryScreen, "closeTopWindow()");
}

void OxideIntegration::destroy(){
    if(m_debug){
        qDebug() << "OxideIntegration::destroy";
    }
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
}

void OxideIntegration::sync(){
    // TODO - get sync state with tarnish
}

void OxideIntegration::beep() const{
    // TODO - show notification?
}


// Dummy font database that does not scan the fonts directory to be
// used for command line tools like qmlplugindump that do not create windows
// unless DebugBackingStore is activated.
class DummyFontDatabase : public QPlatformFontDatabase{
public:
    void populateFontDatabase() override {}
};

QPlatformFontDatabase* OxideIntegration::fontDatabase() const{
    if(!m_fontDatabase){
        if(m_debug){
            qDebug() << "OxideIntegration::fontDatabase";
        }
        if(m_options & EnableFonts){
#if QT_CONFIG(fontconfig)
            m_fontDatabase = new QGenericUnixFontDatabase;
#else
            m_fontDatabase = QPlatformIntegration::fontDatabase();
#endif
        }else{
            m_fontDatabase = new DummyFontDatabase;
        }
    }
    return m_fontDatabase;
}

#ifndef QT_NO_CLIPBOARD
QPlatformClipboard* OxideIntegration::clipboard() const{
    // TODO - implement shared clipboard in tarnish
    return QPlatformIntegration::clipboard();
}
#endif

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

QPlatformServices* OxideIntegration::services() const{
    if(m_debug){
        qDebug() << "OxideIntegration::nativeInterface";
    }
    return const_cast<OxideIntegration*>(this);
}

OxideScreen* OxideIntegration::primaryScreen(){ return m_primaryScreen; }

unsigned short OxideIntegration::options() const { return m_options; }

QStringList OxideIntegration::themeNames() const{
    // TODO - implement custom theme
    return QPlatformIntegration::themeNames();
}

QPlatformTheme* OxideIntegration::createPlatformTheme(const QString& name) const{
    // TODO - implement custom theme
    return QPlatformIntegration::createPlatformTheme(name);
}

bool OxideIntegration::openUrl(const QUrl& url){ return openDocument(url); }

bool OxideIntegration::openDocument(const QUrl& url){
    QStringList args = QProcess::splitCommand("xdg-open " + QLatin1String(url.toEncoded()));
    bool ok = false;
    if (!args.isEmpty()) {
        QString program = args.takeFirst();
        ok = QProcess::startDetached(program, args);
    }
    return ok;
}

QByteArray OxideIntegration::desktopEnvironment() const{ return qgetenv("XDG_CURRENT_DESKTOP"); }

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
