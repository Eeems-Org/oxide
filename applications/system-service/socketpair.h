#pragma once
#include <QLocalSocket>

class SocketPair : public QObject{
    Q_OBJECT

public:
    SocketPair(bool allowWriteSocketRead = false);
    ~SocketPair();
    bool isValid();
    bool isReadable();
    bool isOpen();
    QLocalSocket* readSocket();
    QLocalSocket* writeSocket();
    void setEnabled(bool enabled);
    bool enabled();
    bool atEnd();
    QByteArray readLine(qint64 maxlen = 0);
    QByteArray readAll();
    QByteArray read(qint64 maxlen = 0);
    qint64 bytesAvailable();
    qint64 _write(const char* data, qint64 size);
    qint64 write(const char* data, qint64 size);
    qint64 _write(QByteArray data);
    qint64 write(QByteArray data);
    QString errorString();

signals:
    void readyRead();
    void bytesWritten(qint64 bytes);

public slots:
    void close();

private slots:
    void _readyRead();

private:
    QLocalSocket m_readSocket;
    QLocalSocket m_writeSocket;
    bool m_enabled;
    bool m_allowWriteSocketRead;
    QDataStream m_stream;
};
