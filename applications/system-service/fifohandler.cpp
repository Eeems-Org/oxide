#include "fifohandler.h"

FifoHandler::FifoHandler(QString name, QString path, QObject* host)
    : QObject(),
      host(host),
      _thread(this),
      timer(this),
      _name(name),
      path(path),
      in(),
      out() {
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
    QThread::create([this]{
        out.open(this->path.toStdString().c_str(), std::ifstream::out);
        if(!out.good()){
            O_WARNING("Unable to open fifi (out)" << ::strerror(errno));
        }
    })->start();
    moveToThread(&_thread);
}

FifoHandler::~FifoHandler(){
    if(in.is_open()){
        in.close();
    }
    if(out.is_open()){
        out.close();
    }
    quit();
}

void FifoHandler::start() { _thread.start(); }

void FifoHandler::quit(){ _thread.quit();}

void FifoHandler::write(const void* data, size_t size){
    if(out.is_open()){
        out.write((char*)data, size);
    }
}

const QString& FifoHandler::name() { return _name; }

void FifoHandler::run() {
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

bool FifoHandler::getline_async(std::istream& is, std::string& str, char delim) {
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
