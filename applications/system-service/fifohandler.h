#ifndef FIFOHANDLER_H
#define FIFOHANDLER_H

#include <QDebug>
#include <QThread>
#include <QTimer>

#include <fstream>
#include <liboxide.h>

class FifoHandler : public QObject {
    Q_OBJECT

public:
    FifoHandler(QString name, QString path, QObject* host);
    ~FifoHandler();
    void start();
    void quit();
    void write(const void* data, size_t size);
    const QString& name();

signals:
    void started();
    void finished();
    void dataRecieved(FifoHandler* handler, const QString& data);

protected:
    void run();

private:
    QObject* host;
    QThread _thread;
    QTimer timer;
    QString _name;
    QString path;
    std::ifstream in;
    std::ofstream out;
    bool getline_async(std::istream& is, std::string& str, char delim = '\n');
};

#endif // FIFOHANDLER_H
