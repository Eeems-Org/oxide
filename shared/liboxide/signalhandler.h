/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QObject>
#include <QLocalSocket>
#include <sys/types.h>
/*!
 * \brief signalHandler()
 */
#define signalHandler Oxide::SignalHandler::__singleton()

namespace Oxide {
    /*!
     * \brief A class that allows handling various signals
     * \snippet examples/oxide.cpp SignalHandler
     */
    class LIBOXIDE_EXPORT SignalHandler : public QObject
    {
        Q_OBJECT
    public:
        /*!
         * \brief Setup the unix signal handlers to listen to supported signals
         * \retval SIGTERM Failed to setup SIGTERM
         * \retval SIGINT Failed to setup SIGINT
         * \retval SIGUSR1 Failed to setup SIGUSR1
         * \retval SIGUSR2 Failed to setup SIGUSR2
         * \retval SIGCONT Failed to setup SIGCONT
         * \retval SIGPIPE Failed to setup SIGPIPE
         * \retval 0 Successfully setup both signal handlers
         *
         * This method will automatically create and register the singleton with a parent of qApp.
         */
        static int setup_unix_signal_handlers();
        /*!
         * \brief Get the static instance of this class. You should use the signalHandler macro instead.
         * \return The static instance
         * \sa signalHandler
         */
        static SignalHandler* __singleton();
        /*!
         * \brief Create an instance of SignalHandler.
         * \param parent Optional QObject parent
         *
         * Manually constructing this class will attempt to register it as the singleton instance.
         */
        SignalHandler(QObject *parent = 0);
        ~SignalHandler();
        static void handleSignal(int signal);

    signals:
        /*!
         * \brief The process has recieved a SIGINT
         */
        void sigTerm();
        /*!
         * \brief The process has recieved a SIGINT
         */
        void sigInt();
        /*!
         * \brief The process has recieved a SIGUSR1
         */
        void sigUsr1();
        /*!
         * \brief The process has recieved a SIGUSR2
         */
        void sigUsr2();
        /*!
         * \brief The process has recieved a SIGCONT
         */
        void sigCont();
        /*!
         * \brief The process has recieved a SIGPIPE
         */
        void sigPipe();

    private:
        void addNotifier(int signal, const char* name);
        struct NotifierItem {
            QLocalSocket* notifier;
            int fd;
        };
        static QMap<int, NotifierItem> notifiers;
    };
}
/*! @} */
