#pragma once
#include <QMap>
#include <epframebuffer.h>

#include "apibase.h"
#include "window.h"
#include "guithread.h"

#define guiAPI GuiAPI::__singleton()

using namespace Oxide;

class GUIThread;

class GuiAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GUI_INTERFACE)
    Q_PROPERTY(QRect geometry READ geometry)

public:
    static GuiAPI* __singleton(GuiAPI* self = nullptr);
    GuiAPI(QObject* parent);
    ~GuiAPI();
    void startup();
    void shutdown();
    QRect geometry();
    QRect _geometry();
    void setEnabled(bool enabled);
    bool isEnabled();

    Window* _createWindow(QRect geometry, QImage::Format format);
    Q_INVOKABLE QDBusObjectPath createWindow(int x, int y, int width, int height, int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(QRect geometry, int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QList<QDBusObjectPath> windows();
    Q_INVOKABLE void repaint();
    bool isThisPgId(pid_t valid_pgid);
    QList<Window*> sortedWindows();
    void sortWindows();
    void closeWindows(pid_t pgid);
    void lowerWindows(pid_t pgid);
    void raiseWindows(pid_t pgid);
    void dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Initialize, unsigned int marker = 0, bool async = true);
    GUIThread* guiThread();

    void writeTouchEvent(QEvent* event);
    void writeTabletEvent(QEvent* event);
    void writeKeyEvent(QEvent* event);

public slots:
    void touchEvent(const input_event& event);
    void tabletEvent(const input_event& event);
    void keyEvent(const input_event& event);

private:
    bool m_enabled;
    QMap<QString, Window*> m_windows;
    QRect m_screenGeometry;
    QMutex m_windowMutex;
    QAtomicInteger<unsigned int> m_currentMarker;

    bool hasPermission();
    void removeWindow(QString path);
};
