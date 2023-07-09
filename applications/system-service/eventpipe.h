#pragma once
#include <QLocalSocket>

class EventPipe : public QObject{
    Q_OBJECT

public:
    EventPipe();
    ~EventPipe();
    bool isValid();
    QLocalSocket* readSocket();
    QLocalSocket* writeSocket();
    void setEnabled(bool enabled);
    bool enabled();

public slots:
    bool isOpen();
    void close();
    qint64 write(const char* data, qint64 size);
    bool flush();

private:
    QLocalSocket m_readSocket;
    QLocalSocket m_writeSocket;
    bool m_enabled;
};
