#pragma once

#include "apibase.h"

#include <QTouchEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include <socketpair.h>
#include <liboxide/tarnish.h>

using namespace  Oxide::Tarnish;

class GuiInputThread;

class Window : public QObject{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_WINDOW_INTERFACE)
    Q_PROPERTY(QString identifier READ identifier)
    Q_PROPERTY(int z READ z NOTIFY zChanged)
    Q_PROPERTY(QDBusUnixFileDescriptor frameBuffer READ frameBuffer NOTIFY frameBufferChanged)
    Q_PROPERTY(QDBusUnixFileDescriptor touchEventPipe READ touchEventPipe)
    Q_PROPERTY(QDBusUnixFileDescriptor tabletEventPipe READ tabletEventPipe)
    Q_PROPERTY(QDBusUnixFileDescriptor keyEventPipe READ keyEventPipe)
    Q_PROPERTY(QDBusUnixFileDescriptor eventPipe READ eventPipe)
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(qulonglong sizeInBytes READ sizeInBytes NOTIFY sizeInBytesChanged)
    Q_PROPERTY(qulonglong bytesPerLine READ bytesPerLine NOTIFY bytesPerLineChanged)
    Q_PROPERTY(int format READ format)

public:
    typedef enum{
        Raised,
        Lowered,
        RaisedHidden,
        LoweredHidden
    } WindowState;
    Window(const QString& identifier, const QString& path, const pid_t& pgid, const QRect& geometry, int z, QImage::Format format);
    ~Window();

    void setEnabled(bool enabled);
    QDBusObjectPath path();
    const QString& identifier();
    int z();
    void setZ(int z);
    QDBusUnixFileDescriptor frameBuffer();
    QDBusUnixFileDescriptor touchEventPipe();
    QDBusUnixFileDescriptor tabletEventPipe();
    QDBusUnixFileDescriptor keyEventPipe();
    QDBusUnixFileDescriptor eventPipe();
    QRect geometry();
    QRect _geometry();
    void setGeometry(const QRect& geometry);
    Q_INVOKABLE bool isVisible();
    bool _isVisible();
    Q_INVOKABLE void setVisible(bool visible);
    QImage toImage();
    qulonglong sizeInBytes();
    qulonglong bytesPerLine();
    int format();
    bool writeTouchEvent(const input_event& event);
    bool writeTabletEvent(const input_event& event);
    bool writeKeyEvent(const input_event& event);
    pid_t pgid();
    void _close();
    WindowState state();
    void lock();
    void unlock();
    Q_INVOKABLE void waitForLastUpdate();

    bool operator>(Window* other) const;
    bool operator<(Window* other) const;

public slots:
    QDBusUnixFileDescriptor resize(int width, int height);
    void move(int x, int y);
    Q_NOREPLY void repaint(QRect region, int waveform = EPFrameBuffer::Mono);
    Q_NOREPLY void repaint(int waveform = EPFrameBuffer::Mono);
    void raise();
    void lower();
    void close();

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

private:
    QString m_identifier;
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
    SocketPair m_touchEventPipe;
    SocketPair m_tabletEventPipe;
    SocketPair m_keyEventPipe;
    SocketPair m_eventPipe;

    bool hasPermissions();
    void createFrameBuffer(const QRect& geometry);
    bool writeEvent(SocketPair* pipe, const input_event& event, bool force = false);
    void invalidateEventPipes();
    template<typename T>
    void writeEvent(WindowEventType type, T* args){
        WindowEvent event;
        event.type = type;
        event.setData<T>(*args);
        auto out = m_eventPipe.writeStream();
        out << event;
    }
    void writeEvent(WindowEventType type);
};
