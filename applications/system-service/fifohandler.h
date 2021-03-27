#ifndef FIFOHANDLER_H
#define FIFOHANDLER_H

#include <QDebug>
#include <QThread>
#include <QTimer>

#include <fstream>

class FifoHandler : public QObject {
    Q_OBJECT
public:
    FifoHandler(QString name, QString path, QObject* host)
    : QObject(),
      host(host),
      _thread(this),
      timer(this),
      name(name),
      path(path),
      fifo() {
        connect(host, &QObject::destroyed, this, &QObject::deleteLater);
        connect(&_thread, &QThread::started, [this]{
            auto path = this->path.toStdString();
            emit started();
            std::ifstream stream(path.c_str(), std::ifstream::out);
            fifo.open(path.c_str(), std::ifstream::in);
            stream.close();
            if(!fifo.good()){
                qWarning() << "Unable to open fifi" << ::strerror(errno);
            }
            timer.start(10);
        });
        connect(&_thread, &QThread::finished, this, [this]{
            timer.stop();
            emit finished();
        });
        connect(&timer, &QTimer::timeout, this, &FifoHandler::run);
        moveToThread(&_thread);
    }
    ~FifoHandler(){ quit(); }
    void start() { _thread.start(); }
    void quit(){ _thread.quit(); }
signals:
    void started();
    void finished();
protected:
    void run() {
        if(!fifo.is_open()){
            quit();
            return;
        }
        std::string data;
        while(getline_async(fifo, data)){
            QString line(data.c_str());
            line = line.trimmed();
            handleLine(line);
            thread()->yieldCurrentThread();
        }
        if(fifo.eof()){
            fifo.clear();
        }
        thread()->yieldCurrentThread();
    }
private:
    QObject* host;
    QThread _thread;
    QTimer timer;
    QString name;
    QString path;
    std::ifstream fifo;
    void handleLine(const QString& line);
    bool hasPermission(const QString& name){ return host->property("permissions").toStringList().contains(name); }
    bool getline_async(std::istream& is, std::string& str, char delim = '\n') {

        static std::string lineSoFar;
        char inChar;
        int charsRead = 0;
        bool lineRead = false;
        str = "";

        do {
            charsRead = is.readsome(&inChar, 1);
            if (charsRead == 1) {
                // if the delimiter is read then return the string so far
                if (inChar == delim) {
                    str = lineSoFar;
                    lineSoFar = "";
                    lineRead = true;
                } else {  // otherwise add it to the string so far
                    lineSoFar.append(1, inChar);
                }
            }
        } while (charsRead != 0 && !lineRead);

        return lineRead;
    }
};

#endif // FIFOHANDLER_H
