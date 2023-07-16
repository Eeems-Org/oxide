#pragma once
#include "oxidescreen.h"
#include "oxidewindow.h"

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <QCoreApplication>
#include <QDebug>

QT_BEGIN_NAMESPACE

class OxideIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    enum Options { // Options to be passed on command line or determined from environment
        DebugQPA = 0x1,
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
    QPlatformWindow* createPlatformWindow(QWindow* window) const override;
    QPlatformBackingStore* createPlatformBackingStore(QWindow* window) const override;
    QAbstractEventDispatcher* createEventDispatcher() const override;
    QPlatformNativeInterface* nativeInterface() const override;

    unsigned options() const { return m_options; }

    static OxideIntegration* instance();

private:
    mutable QPlatformFontDatabase* m_fontDatabase;
    QPlatformInputContext* m_inputContext;
    OxideScreen* m_primaryScreen;
    unsigned m_options;
    bool m_debug;
    QStringList m_spec;
    QMutex m_mutex;
    bool m_tabletPenDown;
};

QT_END_NAMESPACE
