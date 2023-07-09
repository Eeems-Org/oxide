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
    FifoHandler(QString name, QString path, QObject* host)
    : QObject(),
      host(host),
      _thread(this),
      timer(this),
      _name(name),
      path(path),
      in(),
      out() {
        _thread.setObjectName(QString("fifo %1").arg(name));
        connect(host, &QObject::destroyed, this, &QObject::deleteLater);
        connect(&_thread, &QThread::started, [this]{
            emit started();
            in.open(this->path.toStdString().c_str(), std::ifstream::in);
            if(!in.good()){
                O_WARNING("Unable to open fifi (in)" << ::strerror(errno));
            }
            timer.start(10);
        });
        connect(&_thread, &QThread::finished, this, [this]{
            timer.stop();
            emit finished();
        });
        connect(&timer, &QTimer::timeout, this, &FifoHandler::run);
        // TODO - sort out if this can be removed
        auto thread = QThread::create([this]{
            out.open(this->path.toStdString().c_str(), std::ifstream::out);
            if(!out.good()){
                O_WARNING("Unable to open fifi (out)" << ::strerror(errno));
            }
        });
        thread->setObjectName(QString("fifo -init %1").arg(name));
        thread->start();
        moveToThread(&_thread);
    }
    ~FifoHandler(){
        if(in.is_open()){
            in.close();
        }
        if(out.is_open()){
            out.close();
        }
        quit();
    }
    void start() { _thread.start(); }
    void quit(){ _thread.quit();}
    void write(const void* data, size_t size){
        if(out.is_open()){
            out.write((char*)data, size);
        }
    }
    const QString& name() { return _name; }

signals:
    void started();
    void finished();
    void dataRecieved(FifoHandler* handler, const QString& data);

protected:
    void run() {
        if(!in.is_open()){
            quit();
            return;
        }
        std::string data;
        while(getline_async(in, data)){
            QString line(data.c_str());
            line = line.trimmed();
            emit dataRecieved(this, line);
            qApp->processEvents(QEventLoop::AllEvents, 100);
            QThread::yieldCurrentThread();
        }
        if(in.eof()){
            in.clear();
        }
        QThread::yieldCurrentThread();
    }

private:
    QObject* host;
    QThread _thread;
    QTimer timer;
    QString _name;
    QString path;
    std::ifstream in;
    std::ofstream out;
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
