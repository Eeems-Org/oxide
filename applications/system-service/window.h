#pragma once
#include "apibase.h"

#include <QTouchEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include <socketpair.h>
#include <liboxide/meta.h>
#include <liboxide/tarnish.h>

using namespace Oxide::Tarnish;

class GuiInputThread;
class Application;

class Window : public QObject{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_WINDOW_INTERFACE)
    Q_PROPERTY(QString identifier READ identifier)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(int z READ z NOTIFY zChanged)
    Q_PROPERTY(QDBusUnixFileDescriptor frameBuffer READ frameBuffer NOTIFY frameBufferChanged)
    Q_PROPERTY(QDBusUnixFileDescriptor eventPipe READ eventPipe)
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(qulonglong sizeInBytes READ sizeInBytes NOTIFY sizeInBytesChanged)
    Q_PROPERTY(qulonglong bytesPerLine READ bytesPerLine NOTIFY bytesPerLineChanged)
    Q_PROPERTY(int format READ format)

protected:
    bool event(QEvent* event) override{
        return QObject::event(event);
    }

public:
    typedef enum{
        Raised,
        Lowered,
        RaisedHidden,
        LoweredHidden,
        Closed
    } WindowState;
    Window(const QString& identifier, const QString& path, const pid_t& pgid, const QString& name, const QRect& geometry, QImage::Format format);
    ~Window();

    void setEnabled(bool enabled);
    QDBusObjectPath path();
    const QString& identifier();
    const QString& name(){ return m_name; }
    int z() const;
    void setZ(int z);
    QDBusUnixFileDescriptor frameBuffer();
    QDBusUnixFileDescriptor eventPipe();
    QRect geometry() const;
    QRect _geometry() const;
    QRect _normalizedGeometry() const;
    void setGeometry(const QRect& geometry);
    Q_INVOKABLE bool isVisible();
    bool _isVisible();
    bool isAppWindow() const;
    bool isAppPaused() const;
    Application* application() const;
    Q_INVOKABLE void setVisible(bool visible);
    void _setVisible(bool visible, bool async = true);
    QImage toImage();
    qulonglong sizeInBytes();
    qulonglong bytesPerLine();
    int format();
    pid_t pgid() const;
    void _repaint(QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool async = true);
    void _raise(bool async = true);
    void _lower(bool async = true);
    void _close();
    WindowState state() const;
    void lock();
    void unlock();
    void waitForUpdate(unsigned int marker, std::function<void()> callback);
    Q_INVOKABLE void waitForLastUpdate(QDBusMessage message);
    void writeEvent(KeyEventArgs args);
    void writeEvent(TouchEventArgs args);
    void writeEvent(TabletEventArgs args);
    void disableEventPipe();
    bool systemWindow();
    void setSystemWindow();

public slots:
    QDBusUnixFileDescriptor resize(int width, int height);
    void move(int x, int y);
    Q_NOREPLY void repaint(QRect region, int waveform = EPFrameBuffer::Mono);
    Q_NOREPLY void repaint(int waveform = EPFrameBuffer::Mono);
    void raise();
    void lower();
    Q_NOREPLY void close();

signals:
    void geometryChanged(const QRect& oldGeometry, const QRect& newGeometry);
    void frameBufferChanged(const QDBusUnixFileDescriptor&);
    void closed();
    void raised();
    void lowered();
    void stateChanged(const int&);
    void sizeInBytesChanged(const qulonglong&);
    void bytesPerLineChanged(const qulonglong&);
    void formatChanged(const int&);
    void zChanged(const int&);

private slots:
    void readyEventPipeRead();
    void ping();
    void pingDeadline();

private:
    QString m_identifier;
    QString m_name;
    bool m_enabled;
    QString m_path;
    pid_t m_pgid;
    QRect m_geometry;
    int m_z;
    QFile m_file;
    uchar* m_data = nullptr;
    qulonglong m_bytesPerLine;
    WindowState m_state;
    QMutex m_mutex;
    QImage::Format m_format;
    SocketPair m_eventPipe;
    QTimer m_pingTimer;
    QTimer m_pingDeadlineTimer;
    unsigned int m_pendingMarker;
    bool m_systemWindow;

    bool hasPermissions() const;
    void createFrameBuffer(const QRect& geometry);
    void writeEvent(WindowEventType type);
    void writeEvent(RepaintEventArgs args);
    void writeEvent(GeometryEventArgs args);
    void writeEvent(ImageInfoEventArgs args);
    void writeEvent(WaitForPaintEventArgs args);
};
