#pragma once
#include "oxidescreen.h"
#include "oxideservices.h"
#include "oxidewindow.h"
#include "oxideeventfilter.h"
#include "oxideeventhandler.h"

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>
#include <QCoreApplication>
#include <QDebug>
#include <liboxide/tarnish.h>

QT_BEGIN_NAMESPACE

class Q_DECL_EXPORT OxideIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    enum Options: unsigned short{ // Options to be passed on command line or determined from environment
        DebugQPA = 0x1,
        EnableFonts = 0x2,
        FreeTypeFontDatabase = 0x4,
        FontconfigDatabase = 0x8
    };

    explicit OxideIntegration(const QStringList &parameters);
    ~OxideIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const override;
    void initialize() override;
    void destroy() override;
    void sync() override;
    void beep() const override;
    QPlatformFontDatabase* fontDatabase() const override;
#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard* clipboard() const override;
#endif
    QPlatformInputContext* inputContext() const override;
    QPlatformWindow* createPlatformWindow(QWindow* window) const override;
    QPlatformBackingStore* createPlatformBackingStore(QWindow* window) const override;
    QAbstractEventDispatcher* createEventDispatcher() const override;
    QPlatformNativeInterface* nativeInterface() const override;
    QPlatformServices* services() const override;
    OxideScreen* primaryScreen();
    unsigned short options() const;
    QStringList themeNames() const override;
    QPlatformTheme* createPlatformTheme(const QString& name) const override;

    static OxideIntegration* instance();

private:
    mutable QPlatformFontDatabase* m_fontDatabase = nullptr;
    QPlatformInputContext* m_inputContext = nullptr;
    QPointer<OxideScreen> m_primaryScreen;
    OxideServices m_services;
    unsigned short m_options;
    bool m_debug;
    QMutex m_mutex;
    static void connectSignal(QObject* sender, QString signal, QObject* reciever, QString slot);
};

QT_END_NAMESPACE
