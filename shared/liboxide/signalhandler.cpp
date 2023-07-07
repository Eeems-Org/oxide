#include "signalhandler.h"
#include "debug.h"

#include <QCoreApplication>

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static bool initialized = false;

namespace Oxide {
    int SignalHandler::setup_unix_signal_handlers(){
        struct sigaction action;
        action.sa_handler = SignalHandler::handleSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        action.sa_flags |= SA_RESTART;
#define _sigaction(signal) \
        if(sigaction(signal, &action, 0)){ \
            return signal; \
        }
        _sigaction(SIGTERM);
        _sigaction(SIGINT);
        _sigaction(SIGUSR1);
        _sigaction(SIGUSR2);
#undef _sigaction
        initialized = false;
        return 0;
    }
    SignalHandler* SignalHandler::singleton(){
        static SignalHandler* instance;
        if(instance == nullptr){
            instance = new SignalHandler(qApp);
        }
        if(!initialized){
            auto res = setup_unix_signal_handlers();
            if(res){
                qFatal(QString("Failed to setup signal handlers: %1").arg(res).toStdString().c_str());
            }
        }
        return instance;
    }
    SignalHandler::SignalHandler(QObject *parent) : QObject(parent){
        addNotifier(SIGTERM, "sigTerm");
        addNotifier(SIGINT, "sigInt");
        addNotifier(SIGUSR1, "sigUsr1");
        addNotifier(SIGUSR2, "sigUsr2");
    }
    SignalHandler::~SignalHandler(){}
    void SignalHandler::handleSignal(int signal){
        if(!notifiers.contains(signal)){
            return;
        }
        ::signal(signal, SIG_IGN);
        O_DEBUG("Signal recieved:" << strsignal(signal));
        auto item = notifiers.value(signal);
        char a = 1;
        ::write(item.fd, &a, sizeof(a));
    }
    void SignalHandler::addNotifier(int signal, const char* name){
        if(!notifiers.contains(signal)){
            int fds[2];
            if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds)){
               qFatal("Couldn't create socketpair");
            }
            notifiers.insert(signal, NotifierItem{
                .notifier = new QSocketNotifier(fds[1], QSocketNotifier::Read, this),
                .fd = fds[0]
            });
        }
        auto notifier = notifiers.value(signal).notifier;
        connect(notifier, &QSocketNotifier::activated, this, [this, notifier, name]{
            O_DEBUG("emitting" << name);
            notifier->setEnabled(false);
            char tmp;
            ::read(notifier->socket(), &tmp, sizeof(tmp));
            if(!QMetaObject::invokeMethod(this, name, Qt::QueuedConnection)){
                O_WARNING("Failed to emit" << name);
            }
            notifier->setEnabled(true);
        }, Qt::QueuedConnection);
    }
    QMap<int, SignalHandler::NotifierItem> SignalHandler::notifiers = QMap<int, SignalHandler::NotifierItem>();
}
