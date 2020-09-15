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
        connect(this, &TarnishHandler::restarted, (Controller*)parent, &Controller::reconnectToAPI);
        m_timer = new QTimer(this);
        m_timer->setInterval(3 * 1000);
        m_timer->setSingleShot(false);
        connect(m_timer, &QTimer::timeout, this, &TarnishHandler::timeout);
        timeout();
        m_timer->start();
    }
    ~TarnishHandler(){
        m_timer->stop();
        delete m_timer;
    }

Q_SIGNALS:
    void restarted();

public slots:
    void timeout(){
        if(system("systemctl --quiet is-active tarnish") && !system("ps | grep tarnish | grep -v grep > /dev/null")){
            qDebug() << "Tarnish died, restarting...";
            system("systemctl start tarnish");
            emit restarted();
        }
    }
private:
    QTimer* m_timer;
};

#endif // TARNISHHANDLER_H
