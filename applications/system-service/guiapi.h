#pragma once
#include <QMap>

#include "apibase.h"
#include "window.h"

#define guiAPI GuiAPI::singleton()

using namespace Oxide;

class InputThread : public QThread {
    Q_OBJECT

public:
    InputThread();

public slots:
    void touchEvent(const input_event& event);
    void tabletEvent(const input_event& event);
    void keyEvent(const input_event& event);
};

class GuiAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GUI_INTERFACE)
    Q_PROPERTY(QRect geometry READ geometry)
public:
    static GuiAPI* singleton(GuiAPI* self = nullptr);
    GuiAPI(QObject* parent);
    ~GuiAPI();
    void startup();
    void shutdown();
    QRect geometry();
    QRect _geometry();
    void setEnabled(bool enabled);
    bool isEnabled();

    Q_INVOKABLE QDBusObjectPath createWindow(int x, int y, int width, int height);
    Q_INVOKABLE QDBusObjectPath createWindow(QRect geometry);
    Q_INVOKABLE QDBusObjectPath createWindow();
    void redraw();
    bool isThisPgId(pid_t valid_pgid);
    QMap<QString, Window*> windows();

protected:
    bool event(QEvent* event) override;

private:
    bool m_enabled;
    bool m_dirty;
    QMap<QString, Window*> m_windows;
    struct Repaint {
        Window* window;
        QRect region;
    };
    QList<Repaint> m_repaintList;
    QRect m_screenGeometry;
    void scheduleUpdate();
    void sortWindows();
    InputThread m_thread;
};
