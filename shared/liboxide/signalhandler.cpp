#include "signalhandler.h"

#include <QCoreApplication>

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int sigUsr1Fd[2];
static int sigUsr2Fd[2];

namespace Oxide {
    int SignalHandler::setup_unix_signal_handlers(){
        if(!signalHandler){
            new SignalHandler(qApp);
        }
        struct sigaction usr1, usr2;

        usr1.sa_handler = SignalHandler::usr1SignalHandler;
        sigemptyset(&usr1.sa_mask);
        usr1.sa_flags = 0;
        usr1.sa_flags |= SA_RESTART;
        if(sigaction(SIGUSR1, &usr1, 0)){
            return 1;
        }

        usr2.sa_handler = SignalHandler::usr2SignalHandler;
        sigemptyset(&usr2.sa_mask);
        usr2.sa_flags = 0;
        usr2.sa_flags |= SA_RESTART;
        if(sigaction(SIGUSR2, &usr2, 0)){
            return 2;
        }

        return 0;
    }
    SignalHandler* SignalHandler::singleton(SignalHandler* self){
        static SignalHandler* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    SignalHandler::SignalHandler(QObject *parent) : QObject(parent){
        singleton(this);
        if(::socketpair(AF_UNIX, SOCK_STREAM, 0, sigUsr1Fd)){
           qFatal("Couldn't create USR1 socketpair");
        }
        if(::socketpair(AF_UNIX, SOCK_STREAM, 0, sigUsr2Fd)){
           qFatal("Couldn't create USR2 socketpair");
        }

        snUsr1 = new QSocketNotifier(sigUsr1Fd[1], QSocketNotifier::Read, this);
        connect(snUsr1, &QSocketNotifier::activated, this, &SignalHandler::handleSigUsr1, Qt::QueuedConnection);
        snUsr2 = new QSocketNotifier(sigUsr2Fd[1], QSocketNotifier::Read, this);
        connect(snUsr2, &QSocketNotifier::activated, this, &SignalHandler::handleSigUsr2, Qt::QueuedConnection);
    }
    SignalHandler::~SignalHandler(){}
    void SignalHandler::usr1SignalHandler(int unused){
        Q_UNUSED(unused)
        char a = 1;
        ::write(sigUsr1Fd[0], &a, sizeof(a));
    }
    void SignalHandler::usr2SignalHandler(int unused){
        Q_UNUSED(unused)
        char a = 1;
        ::write(sigUsr2Fd[0], &a, sizeof(a));
    }
    void SignalHandler::handleSigUsr1(){
        snUsr1->setEnabled(false);
        char tmp;
        ::read(sigUsr1Fd[1], &tmp, sizeof(tmp));
        emit sigUsr1();
        snUsr1->setEnabled(true);
    }
    void SignalHandler::handleSigUsr2(){
        snUsr2->setEnabled(false);
        char tmp;
        ::read(sigUsr2Fd[1], &tmp, sizeof(tmp));
        emit sigUsr2();
        snUsr2->setEnabled(true);
    }
}

#include "moc_signalhandler.cpp"
