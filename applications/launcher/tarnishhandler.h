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
    TarnishHandler() : QObject(){
        timeout();
    }
    ~TarnishHandler(){
        m_timer->stop();
        delete m_timer;
    }
    void attach(Controller* controller){
        qDebug() << "Attaching TarnishHandler to Controller";
        if(m_timer == nullptr){
            m_timer = new QTimer(this);
            m_timer->setInterval(3 * 1000);
            m_timer->setSingleShot(false);
            connect(m_timer, &QTimer::timeout, this, &TarnishHandler::timeout);
        }
        if(!m_timer->isActive()){
            m_timer->start();
        }
        connect(this, &TarnishHandler::restarted, controller, &Controller::reconnectToAPI);
        emit restarted();
    }

Q_SIGNALS:
    void restarted();

public slots:
    void timeout(){
        if(system("systemctl --quiet is-active tarnish") && system("ps | grep tarnish | grep -v grep > /dev/null")){
            qDebug() << "Tarnish died, restarting...";
            system("systemctl start tarnish");
            emit restarted();
        }
    }
private:
    QTimer* m_timer;
};

#endif // TARNISHHANDLER_H
