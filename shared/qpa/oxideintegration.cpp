#include "oxideintegration.h"
#include "oxidebackingstore.h"
#include "oxideeventfilter.h"
#include "oxideeventmanager.h"
#include "oxidescreen.h"

#include <QMetaMethod>
#include <QMimeData>
#include <QProcess>

#include <cstring>
#include <libblight.h>
#include <libblight/meta.h>
#include <liboxide/devicesettings.h>
#include <liboxide/dbus.h>

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <qpa/qplatformfontdatabase.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatformwindow.h>
#include <private/qgenericunixeventdispatcher_p.h>
#include <private/qgenericunixfontdatabase_p.h>

using namespace codes::eeems::blight1;

QT_BEGIN_NAMESPACE

class QCoreTextFontEngine;

#define debugQPAEnvironmentVariable "OXIDE_QPA_DEBUG"


static inline unsigned short parseOptions(const QStringList& paramList){
    unsigned options = 0;
    for(const QString& param : paramList){
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
  m_parameters(parameters)
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
#ifdef EPAPER
    Blight::connect(true);
#else
    Blight::connect(false);
#endif
    auto connection = Blight::connection();
    if(connection == nullptr){
        qFatal("Could not connect to display server: %s", std::strerror(errno));
    }
    connection->onDisconnect([](int res){
        if(res){
            qApp->exit(res);
        }
    });
    QWindowSystemInterfacePrivate::TabletEvent::setPlatformSynthesizesMouse(true);
    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents);
    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents);
#ifndef QT_NO_CLIPBOARD
    m_clipboard = new QMimeData();
    m_selection = new QMimeData();
#endif
    m_primaryScreen = new OxideScreen();
    // rM1 geometry as default
    m_primaryScreen->setGeometry(QRect(0, 0, 1404, 1872));
    QWindowSystemInterface::handleScreenAdded(m_primaryScreen, true);
    qApp->installEventFilter(new OxideEventFilter(qApp));
    m_inputContext = QPlatformInputContextFactory::create();
    new OxideEventManager(m_parameters);
#ifndef QT_NO_CLIPBOARD
    auto compositor = new Compositor(
        BLIGHT_SERVICE,
        "/",
#ifdef EPAPER
        QDBusConnection::systemBus()
#else
        QDBusConnection::sessionBus()
#endif
    );
    connect(compositor, &Compositor::clipboardChanged, this, [this](const QByteArray& data){
        m_clipboard->setData("application/octet-stream", data);
    });
    m_clipboard->setData("application/octet-stream", compositor->clipboard());
    connect(compositor, &Compositor::selectionChanged, this, [this](const QByteArray& data){
        m_selection->setData("application/octet-stream", data);
    });
    m_selection->setData("application/octet-stream", compositor->selection());
#endif
}

void OxideIntegration::destroy(){
    if(m_debug){
        qDebug() << "OxideIntegration::destroy";
    }
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
}

void OxideIntegration::sync(){
    // TODO - get sync state with display server
}

void OxideIntegration::beep() const{
    // TODO - show notification?
}


// Dummy font database that does not scan the fonts directory to be
// used for command line tools like qmlplugindump that do not create windows
// unless DebugBackingStore is activated.
class DummyFontDatabase
: public QPlatformFontDatabase
#ifndef QT_NO_CLIPBOARD
  , public QPlatformClipboard
#endif
{
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
    if(m_debug){
        qDebug() << "OxideIntegration::nativeInterface";
    }
    return const_cast<OxideIntegration*>(this);
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

#ifndef Q_NO_CLIPBOARD
QMimeData* OxideIntegration::mimeData(QClipboard::Mode mode){
    switch(mode){
        case QClipboard::Clipboard:
            return m_clipboard.data();
        case QClipboard::Selection:
            return m_selection.data();
        default:
            return nullptr;
    }
}

void OxideIntegration::setMimeData(QMimeData* data, QClipboard::Mode mode){
    switch(mode){
        case QClipboard::Clipboard:{
            m_clipboard = data;
            auto clipboard = Blight::clipboard();
            auto constData = data->data("");
            clipboard->set(constData, constData.size());
            QPlatformClipboard::emitChanged(mode);
            break;
        }
        case QClipboard::Selection:{
            m_selection = data;
            auto selection = Blight::selection();
            auto constData = data->data("");
            selection->set(constData, constData.size());
            QPlatformClipboard::emitChanged(mode);
            break;
        }
        default:
            break;
    }
}

bool OxideIntegration::supportsMode(QClipboard::Mode mode) const{
    return mode == QClipboard::Clipboard || mode == QClipboard::Selection;
}

bool OxideIntegration::ownsMode(QClipboard::Mode mode) const{
    Q_UNUSED(mode)
    return false;
}
#endif

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
        qFatal("The index of the signal: %s", signal.toStdString().c_str());
    }
    auto signalMethod = sender->metaObject()->method(signalIndex);
    if(!signalMethod.isValid()){
        qFatal("Cannot find signal from index: %s", signal.toStdString().c_str());
    }
    auto slotIndex = reciever->metaObject()->indexOfSlot(slot.toStdString().c_str());
    if(slotIndex == -1){
        qFatal("The index of the slot: %s", slot.toStdString().c_str());
    }
    auto slotMethod = reciever->metaObject()->method(slotIndex);
    if(!slotMethod.isValid()){
        qFatal("Cannot find slot from index: %s", slot.toStdString().c_str());
    }
    QObject::connect(sender, signalMethod, reciever, slotMethod);
}

QT_END_NAMESPACE
