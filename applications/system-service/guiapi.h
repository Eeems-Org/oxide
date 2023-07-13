#pragma once
#include <QMap>
#include <epframebuffer.h>

#include "apibase.h"
#include "window.h"
#include "guithread.h"

#define guiAPI GuiAPI::singleton()

class RepaintNotifier : public QObject{
    Q_OBJECT

signals:
    void repainted();
};

using namespace Oxide;

class GuiAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GUI_INTERFACE)
    Q_PROPERTY(QRect geometry READ geometry)

public:
    QMutex m_repaintMutex;
    static GuiAPI* singleton(GuiAPI* self = nullptr);
    GuiAPI(QObject* parent);
    ~GuiAPI();
    void startup();
    void shutdown();
    QRect geometry();
    QRect _geometry();
    void setEnabled(bool enabled);
    bool isEnabled();

    Window* _createWindow(QRect geometry, QImage::Format format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(int x, int y, int width, int height, int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(QRect geometry, int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QDBusObjectPath createWindow(int format = DEFAULT_IMAGE_FORMAT);
    Q_INVOKABLE QList<QDBusObjectPath> windows();
    Q_INVOKABLE void repaint();
    bool isThisPgId(pid_t valid_pgid);
    QMap<QString, Window*> allWindows();
    QList<Window*> sortedWindows();
    void closeWindows(pid_t pgid);
    void waitForLastUpdate();
    void dirty(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Initialize);

public slots:
    void touchEvent(const input_event& event);
    void tabletEvent(const input_event& event);
    void keyEvent(const input_event& event);

private:
    bool m_enabled;
    QMap<QString, Window*> m_windows;
    RepaintNotifier m_repaintNotifier;
    QRect m_screenGeometry;
    void sortWindows();
    bool hasPermission();
};
