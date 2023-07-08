#include "oxideintegration.h"
#include "oxidebackingstore.h"
#include "oxidescreen.h"
#include "oxidetabletdata.h"
#include "oxidetouchscreendata.h"

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
    m_inputContext = QPlatformInputContextFactory::create();
    auto touchData = new OxideTouchScreenData(m_spec);
    touchData->m_singleTouch = !deviceSettings.supportsMultiTouch();
    touchData->m_typeB = deviceSettings.isTouchTypeB();
    touchData->hw_range_x_max = deviceSettings.getTouchWidth();
    touchData->hw_range_y_max = deviceSettings.getTouchHeight();
    touchData->hw_pressure_max = deviceSettings.getTouchPressure();
    touchData->hw_name = "oxide";
    int rotation = 0;
    bool invertx = false;
    bool inverty = false;
    for(auto item : QString::fromLatin1(deviceSettings.getTouchEnvSetting()).split(":")){
        auto spec = item.split("=");
        auto key = spec.first();
        if(key == "invertx"){
            invertx = true;
            continue;
        }
        if(key == "inverty"){
            inverty = true;
            continue;
        }
        if(key != "rotate" || spec.count() != 2){
            continue;
        }
        QString value = spec.last();
        bool ok;
        uint angle = value.toUInt(&ok);
        if(!ok){
            return;
        }
        switch(angle){
            case 90:
            case 180:
            case 270:
                rotation = angle;
            default:
            break;
        }
    }
    if(rotation){
        touchData->m_rotate = QTransform::fromTranslate(0.5, 0.5).rotate(rotation).translate(-0.5, -0.5);
    }
    if(invertx){
        touchData->m_rotate *= QTransform::fromTranslate(0.5, 0.5).scale(-1.0, 1.0).translate(-0.5, -0.5);
    }
    if(inverty){
        touchData->m_rotate *= QTransform::fromTranslate(0.5, 0.5).scale(1.0, -1.0).translate(-0.5, -0.5);
    }
    auto device = new QTouchDevice();
    device->setName("oxide");
    device->setType(QTouchDevice::TouchScreen);
    device->setCapabilities(QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure);
    if(touchData->hw_pressure_max){
        device->setCapabilities(device->capabilities() | QTouchDevice::Pressure);
    }
    QWindowSystemInterface::registerTouchDevice(device);
    touchData->m_device = device;
    auto tabletData = new OxideTabletData(Oxide::Tarnish::getTabletEventPipeFd());
    tabletData->maxValues.p = deviceSettings.getWacomPressure();
    tabletData->maxValues.x = deviceSettings.getWacomWidth();
    tabletData->maxValues.y = deviceSettings.getWacomHeight();
    auto keyPipe = QFdContainer(Oxide::Tarnish::getKeyEventPipeFd());
    new QEvdevKeyboardHandler("oxide", keyPipe, false, false, "");
    auto connected = Oxide::Tarnish::connectQtEvents(
        [touchData](input_event event){
            touchData->processInputEvent(&event);
        },
        [tabletData](input_event event){
            tabletData->processInputEvent(&event);
        },
        nullptr
    );
    if(m_debug){
        if(!connected){
            qWarning() << "OxideIntegration::initialize Failed to connect Qt events";
        }else if(m_debug){
            qDebug() << "OxideIntegration::initialize Qt events connected";
        }
    }
}


// Dummy font database that does not scan the fonts directory to be
// used for command line tools like qmlplugindump that do not create windows
// unless DebugBackingStore is activated.
class DummyFontDatabase : public QPlatformFontDatabase{
public:
    virtual void populateFontDatabase() override {}
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
