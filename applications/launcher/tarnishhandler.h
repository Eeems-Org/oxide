#ifndef TARNISHHANDLER_H
#define TARNISHHANDLER_H

#include <QObject>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QTimer>

#include <unistd.h>
#include <signal.h>

#include "controller.h"

class TarnishHandler : public QObject {
    Q_OBJECT
public:
    TarnishHandler(QObject* parent) : QObject(parent){
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(m_process, &QProcess::errorOccurred, this, &TarnishHandler::errorOccurred);
        connect(m_process, &QProcess::stateChanged, this, &TarnishHandler::stateChanged);
        connect(m_process, QOverload<int>::of(&QProcess::finished), this, &TarnishHandler::finished);
        connect(m_process, &QProcess::started, this, &TarnishHandler::started);
        connect(this, &TarnishHandler::restarted, (Controller*)parent, &Controller::reconnectToAPI);
        m_process->setArguments(QStringList() << std::to_string(getpid()).c_str());
        qDebug() << "Looking for tarnish";
        for(auto tarnish : QList<QString> { "/opt/bin/tarnish", "/bin/tarnish", "/home/root/.local/tarnish"}){
            qDebug() << "  " << tarnish.toStdString().c_str();
            if(QFile(tarnish).exists()){
                qDebug() << "   Found";
                m_process->setProgram(tarnish);
                m_process->start();
                break;
            }
        }
        m_timer = new QTimer(this);
        m_timer->setInterval(3 * 1000);
        m_timer->setSingleShot(false);
        connect(m_timer, &QTimer::timeout, this, &TarnishHandler::timeout);
        m_timer->start();
    }
    ~TarnishHandler(){
        m_timer->stop();
        delete m_timer;
        if(processId()){
            qDebug() << "Killing tarnish";
            kill(processId(), SIGTERM);
            m_process->waitForFinished();
            if(processId()){
                m_process->kill();
            }
        }
        delete m_process;
    }
    int processId() { return m_process->processId(); }
    void waitForFinished() { m_process->waitForFinished(); }

Q_SIGNALS:
    void restarted();

public slots:
    void errorOccurred(QProcess::ProcessError error){
        switch(error){
            case QProcess::Crashed:
                qDebug() << "tarnish crashed";
            break;
        case QProcess::FailedToStart:
            qDebug() << "tarnish failed to start";
        case QProcess::Timedout:
            qDebug() << "tarnish timed out";
        break;
        case QProcess::WriteError:
            qDebug() << "Error writing to tarnish";
        break;
        case QProcess::ReadError:
            qDebug() << "Error reading from tarnish";
        break;
        break;
            default:
                qDebug() << "Unknown error with tarnish:" << error;
        }
    }
    void stateChanged(QProcess::ProcessState newState){
        switch(newState){
            case QProcess::NotRunning:
                qDebug() << "Tarnish is not running";
            break;
            case QProcess::Starting:
                qDebug() << "Tarnish is starting";
            break;
            case QProcess::Running:
                qDebug() << "Tarnish is running";
            break;
            default:
                qDebug() << "Unknown state for tarnish:" << newState;
        }
    }
    void finished(int exitCode){
        qDebug() << "Tarnished exited " << exitCode;
    }
    void started(){
        emit restarted();
    }
    void timeout(){
        if(!processId()){
            qDebug() << "Tarnish died, restarting...";
            m_process->start();
            emit restarted();
        }
    }
private:
    QProcess* m_process;
    QTimer* m_timer;
};

#endif // TARNISHHANDLER_H
