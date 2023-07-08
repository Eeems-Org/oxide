#pragma once

#include "apibase.h"

#include <QLocalSocket>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QKeyEvent>

class EventPipe : QObject{
    Q_OBJECT

public:
    EventPipe(QObject* parent);
    ~EventPipe();
    bool isValid();
    bool isOpen();
    void close();
    QLocalSocket* readSocket();
    QLocalSocket* writeSocket();
    qint64 write(const char* data, qint64 size);
    bool flush();

private:
    QLocalSocket m_readSocket;
    QLocalSocket m_writeSocket;
};

class Window : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_WINDOW_INTERFACE)
    Q_PROPERTY(QString identifier READ identifier)
    Q_PROPERTY(QDBusUnixFileDescriptor frameBuffer READ frameBuffer NOTIFY frameBufferChanged)
    Q_PROPERTY(QDBusUnixFileDescriptor touchEventPipe READ touchEventPipe)
    Q_PROPERTY(QDBusUnixFileDescriptor tabletEventPipe READ tabletEventPipe)
    Q_PROPERTY(QDBusUnixFileDescriptor keyEventPipe READ keyEventPipe)
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
    Window(const QString& identifier, const QString& path, const pid_t& pid, const QRect& geometry, QObject* parent);
    ~Window();

    void setEnabled(bool enabled);
    QDBusObjectPath path();
    const QString& identifier();
    QDBusUnixFileDescriptor frameBuffer();
    QDBusUnixFileDescriptor touchEventPipe();
    QDBusUnixFileDescriptor tabletEventPipe();
    QDBusUnixFileDescriptor keyEventPipe();
    QRect geometry();
    QRect _geometry();
    void setGeometry(const QRect& geometry);
    Q_INVOKABLE bool isVisible();
    Q_INVOKABLE void setVisible(bool visible);
    QImage toImage();
    qulonglong sizeInBytes();
    qulonglong bytesPerLine();
    int format();
    bool writeTouchEvent(const input_event& event);
    bool writeTabletEvent(const input_event& event);
    bool writeKeyEvent(const input_event& event);

public slots:
    QDBusUnixFileDescriptor resize(int width, int height);
    void move(int x, int y);
    void repaint(QRect region);
    void repaint();
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
    void dirty(const QRect& region);

private:
    QString m_identifier;
    bool m_enabled;
    QString m_path;
    pid_t m_pid;
    QRect m_geometry;
    uchar* m_data = nullptr;
    QFile m_file;
    qulonglong m_bytesPerLine;
    WindowState m_state;
    QMutex mutex;
    QImage::Format m_format;
    EventPipe m_touchEventPipe;
    EventPipe m_tabletEventPipe;
    EventPipe m_keyEventPipe;

    bool hasPermissions();
    void createFrameBuffer(const QRect& geometry);
    bool _isVisible();
    bool writeEvent(EventPipe* pipe, const input_event& event);
};
