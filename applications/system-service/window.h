#pragma once

#include "apibase.h"

class Window : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_WINDOW_INTERFACE)
    Q_PROPERTY(QString identifier READ identifier)
    Q_PROPERTY(QDBusUnixFileDescriptor frameBuffer READ frameBuffer NOTIFY frameBufferChanged)
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(qulonglong sizeInBytes READ sizeInBytes NOTIFY sizeInBytesChanged)
    Q_PROPERTY(qulonglong bytesPerLine READ bytesPerLine NOTIFY bytesPerLineChanged)
    Q_PROPERTY(int format READ format NOTIFY formatChanged)

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
    QRect geometry();
    QRect _geometry();
    void setGeometry(const QRect& geometry);
    Q_INVOKABLE bool isVisible();
    Q_INVOKABLE void setVisible(bool visible);
    QImage* toImage();
    qulonglong sizeInBytes();
    qulonglong bytesPerLine();
    int format();

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
    int m_fd;
    uchar* m_data = nullptr;
    QImage m_image;
    WindowState m_state;
    QMutex mutex;

    bool hasPermissions();
    void createFrameBuffer(const QRect& geometry);
    bool _isVisible();
};
