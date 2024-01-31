#pragma once
#include "oxidescreen.h"

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformservices.h>
#include <qpa/qplatformclipboard.h>
#include <QCoreApplication>
#include <QDebug>
#include <QPointer>
#include <libblight/connection.h>

QT_BEGIN_NAMESPACE

class Q_DECL_EXPORT OxideIntegration
: public QPlatformIntegration,
  public QPlatformNativeInterface,
  public QPlatformServices
#ifndef QT_NO_CLIPBOARD
  , public QPlatformClipboard
#endif
{
public:
    enum Option: unsigned short{ // Options to be passed on command line or determined from environment
        DebugQPA = 0x1,
        EnableFonts = 0x2,
        FreeTypeFontDatabase = 0x4,
        FontconfigDatabase = 0x8
    };
    Q_DECLARE_FLAGS(Options, Option)

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
    QFunctionPointer platformFunction(const QByteArray& function) const override;
    OxideScreen* primaryScreen();
    unsigned short options() const;
    QStringList themeNames() const override;
    QPlatformTheme* createPlatformTheme(const QString& name) const override;
    bool openUrl(const QUrl& url) override;
    bool openDocument(const QUrl& url) override;
    QByteArray desktopEnvironment() const override;
#ifndef QT_NO_CLIPBOARD
    QMimeData* mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData* data, QClipboard::Mode mode = QClipboard::Clipboard) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;
#endif
    static OxideIntegration* instance();
    static Blight::shared_buf_t getSurfaceForWindowStatic(QWindow* qwindow);
    static QImage getImageForWindowStatic(QWindow* qwindow);

private:
    mutable QPlatformFontDatabase* m_fontDatabase = nullptr;
    QPlatformInputContext* m_inputContext = nullptr;
    QPointer<OxideScreen> m_primaryScreen;
#ifndef QT_NO_CLIPBOARD
    QPointer<QMimeData> m_clipboard;
    QPointer<QMimeData> m_selection;
#endif
    unsigned short m_options;
    bool m_debug;
    QMutex m_mutex;
    QStringList m_parameters;
    static void connectSignal(QObject* sender, QString signal, QObject* reciever, QString slot);
};

QT_END_NAMESPACE
