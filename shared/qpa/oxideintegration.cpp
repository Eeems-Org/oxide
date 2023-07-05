#include "oxideintegration.h"
#include "oxidebackingstore.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <qpa/qplatformfontdatabase.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatformwindow.h>

#include <private/qevdevmousemanager_p.h>
#include <private/qevdevtouchmanager_p.h>
#include <private/qgenericunixeventdispatcher_p.h>
#include <private/qgenericunixfontdatabase_p.h>

QT_BEGIN_NAMESPACE

class QCoreTextFontEngine;

static const char debugBackingStoreEnvironmentVariable[] = "QT_DEBUG_BACKINGSTORE";

static inline unsigned parseOptions(const QStringList &paramList){
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
    return options;
}

OxideIntegration::OxideIntegration(const QStringList &parameters): m_fontDatabase(nullptr), m_options(parseOptions(parameters)){
    if(
        qEnvironmentVariableIsSet(debugBackingStoreEnvironmentVariable)
        && qEnvironmentVariableIntValue(debugBackingStoreEnvironmentVariable) > 0
    ) {
        m_options |= DebugBackingStore | EnableFonts;
    }

    m_primaryScreen = new OxideScreen();

    m_primaryScreen->mGeometry = QRect(0, 0, 240, 320);
    m_primaryScreen->mDepth = 32;
    m_primaryScreen->mFormat = QImage::Format_ARGB32_Premultiplied;

    QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
}

OxideIntegration::~OxideIntegration(){
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
    delete m_fontDatabase;
}

bool OxideIntegration::hasCapability(QPlatformIntegration::Capability cap) const{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case MultipleWindows: return true;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

// Dummy font database that does not scan the fonts directory to be
// used for command line tools like qmlplugindump that do not create windows
// unless DebugBackingStore is activated.
class DummyFontDatabase : public QPlatformFontDatabase{
public:
    virtual void populateFontDatabase() override {}
};

QPlatformFontDatabase *OxideIntegration::fontDatabase() const{
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

QPlatformWindow *OxideIntegration::createPlatformWindow(QWindow *window) const{
    Q_UNUSED(window);
    QPlatformWindow *w = new QPlatformWindow(window);
    w->requestActivateWindow();
    return w;
}

QPlatformBackingStore *OxideIntegration::createPlatformBackingStore(QWindow *window) const{ return new OxideBackingStore(window); }

QAbstractEventDispatcher *OxideIntegration::createEventDispatcher() const{ return createUnixEventDispatcher(); }

QPlatformNativeInterface *OxideIntegration::nativeInterface() const{
    if(!m_nativeInterface){
        m_nativeInterface.reset(new QPlatformNativeInterface);
    }
    return m_nativeInterface.get();
}

OxideIntegration *OxideIntegration::instance(){ return static_cast<OxideIntegration *>(QGuiApplicationPrivate::platformIntegration()); }

QT_END_NAMESPACE
