#pragma once

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformscreen.h>

#include <qscopedpointer.h>

QT_BEGIN_NAMESPACE

class OxideScreen : public QPlatformScreen
{
public:
    OxideScreen() : mDepth(32), mFormat(QImage::Format_RGB16) {}
    QRect geometry() const override { return mGeometry; }
    int depth() const override { return mDepth; }
    QImage::Format format() const override { return mFormat; }
    QSizeF physicalSize() const override{
        static const int dpi = 228;
        return QSizeF(geometry().size()) / dpi * qreal(25.4);
    }

public:
    QRect mGeometry;
    int mDepth;
    QImage::Format mFormat;
    QSize mPhysicalSize;
};

class OxideIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    enum Options { // Options to be passed on command line or determined from environment
        DebugBackingStore = 0x1,
        EnableFonts = 0x2,
        FreeTypeFontDatabase = 0x4,
        FontconfigDatabase = 0x8
    };

    explicit OxideIntegration(const QStringList &parameters);
    ~OxideIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const override;
    void initialize() override;
    QPlatformFontDatabase* fontDatabase() const override;
    QPlatformInputContext* inputContext() const override;
    QPlatformWindow* createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore* createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher* createEventDispatcher() const override;
    QPlatformNativeInterface* nativeInterface() const override;

    unsigned options() const { return m_options; }

    static OxideIntegration* instance();

private:
    mutable QPlatformFontDatabase* m_fontDatabase;
    QPlatformInputContext* m_inputContext;
    OxideScreen* m_primaryScreen;
    unsigned m_options;
};

QT_END_NAMESPACE
