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

    Window* _createWindow(QString name, QRect geometry, QImage::Format format);
    Q_INVOKABLE QDBusObjectPath createWindow(int x, int y, int width, int height, QString name = "", int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(QRect geometry, QString name = "", int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(QString name = "", int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QVector<QDBusObjectPath> windows();
    Q_INVOKABLE QVector<QDBusObjectPath> visibleWindows();
    Q_INVOKABLE QVector<QDBusObjectPath> allWindows();
    Q_INVOKABLE QVector<QDBusObjectPath> allVisibleWindows();
    Q_INVOKABLE void repaint();
    bool isThisPgId(pid_t valid_pgid);
    bool hasWindowPermission();
    QVector<Window*> _sortedWindows();
    QVector<Window*> _sortedVisibleWindows();
    void sortWindows();
    void closeWindows(pid_t pgid);
    void lowerWindows(pid_t pgid);
    void raiseWindows(pid_t pgid);
    bool hasRaisedWindows(pid_t pgid);
    void dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Initialize, unsigned int marker = 0, bool async = true);
    GUIThread* guiThread();
    void removeWindow(QString path);
    void writeTouchEvent(QEvent* event);
    void writeTabletEvent(QEvent* event);
    void writeKeyEvent(QEvent* event);

private:
    bool m_enabled;
    QMap<QString, Window*> m_windows;
    QRect m_screenGeometry;
    QMutex m_windowMutex;
    QAtomicInteger<unsigned int> m_currentMarker;

    bool hasPermission();
};
