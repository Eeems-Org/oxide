/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QObject>
#include <QSocketNotifier>
/*!
 * \brief signalHandler()
 */
#define signalHandler Oxide::SignalHandler::singleton()

namespace Oxide {
    /*!
     * \brief A class that allows handling SIGUSR1 and SIGUSR2
     * \snippet examples/oxide.cpp SignalHandler
     */
    class LIBOXIDE_EXPORT SignalHandler : public QObject
    {
        Q_OBJECT
    public:
        /*!
         * \brief Setup the unix signal handlers to listen to SIGUSR1 and SIGUSR2
         * \retval 1 Failed to setup SIGUSR1
         * \retval 2 Failed to setup SIGUSR2
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
        static SignalHandler* singleton(SignalHandler* self = nullptr);
        /*!
         * \brief Create an instance of SignalHandler.
         * \param parent Optional QObject parent
         *
         * Manually constructing this class will attempt to register it as the singleton instance.
         */
        SignalHandler(QObject *parent = 0);
        ~SignalHandler();
        static void usr1SignalHandler(int unused);
        static void usr2SignalHandler(int unused);

    public slots:
        void handleSigUsr1();
        void handleSigUsr2();

    signals:
        /*!
         * \brief The process has recieved a SIGUSR1
         */
        void sigUsr1();
        /*!
         * \brief The process has recieved a SIGUSR2
         */
        void sigUsr2();

    private:
        QSocketNotifier* snUsr1;
        QSocketNotifier* snUsr2;
    };
}
/*! @} */
