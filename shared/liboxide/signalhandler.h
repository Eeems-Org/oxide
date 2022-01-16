#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H

#include "liboxide_global.h"

#include <QObject>
#include <QSocketNotifier>

#define signalHandler Oxide::SignalHandler::singleton()

namespace Oxide {
    class LIBOXIDE_EXPORT SignalHandler : public QObject
    {
        Q_OBJECT
    public:
        static int setup_unix_signal_handlers();
        static SignalHandler* singleton(SignalHandler* self = nullptr);
        SignalHandler(QObject *parent = 0);
        ~SignalHandler();
        static void usr1SignalHandler(int unused);
        static void usr2SignalHandler(int unused);

    public slots:
        void handleSigUsr1();
        void handleSigUsr2();

    signals:
        void sigUsr1();
        void sigUsr2();

    private:
        QSocketNotifier* snUsr1;
        QSocketNotifier* snUsr2;
    };
}
#endif // SIGNALHANDLER_H
