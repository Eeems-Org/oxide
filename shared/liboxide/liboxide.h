/*!
 * \addtogroup Oxide
 * \brief The main Oxide module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include "meta.h"
#include "dbus.h"
#include "applications.h"
#include "settingsfile.h"
#include "power.h"
#include "json.h"
#include "signalhandler.h"
#include "slothandler.h"
#include "sysobject.h"
#include "debug.h"
#include "devicesettings.h"
#include "xochitlsettings.h"
#include "sharedsettings.h"
#include "threading.h"
#if defined(LIBOXIDE_LIBRARY)
#include "oxide_sentry.h"
#else
#include "sentry.h"
#endif

#include <QDebug>
#include <QScopeGuard>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QThread>

#include <sys/types.h>
#ifndef VERSION
#ifdef APP_VERSION
#define VERSION APP_VERSION
#else
#define VERSION "2.7"
#endif
#endif
#ifndef APP_VERSION
#define APP_VERSION VERSION
#endif

/*!
 * \brief The main Oxide namespace
 */
namespace Oxide {
    /*!
     * \brief Execute a program and return it's output
     * \param program Program to run
     * \param args Arguments to pass to the program
     * \param readStderr Include stderr in the output
     * \return Output if it ran.
     * \retval NULL Program was not able to execute
     */
    LIBOXIDE_EXPORT QString execute(
        const QString& program,
        const QStringList& args,
        bool readStderr = true
    );
    /*!
     * \brief Try to get a lock
     * \param lockName Path to the lock file
     * \return File descriptor of the lock file
     * \retval -1 Unable to get lock
     */
    LIBOXIDE_EXPORT int tryGetLock(char const *lockName);
    /*!
     * \brief Release a lock file
     * \param fd File descriptor of the lock file
     * \param lockName Path to the lock file
     */
    LIBOXIDE_EXPORT void releaseLock(int fd, char const* lockName);
    /*!
     * \brief Checks to see if a process exists
     * \return If the process exists
     */
    LIBOXIDE_EXPORT bool processExists(pid_t pid);
    /*!
     * \brief Get list of pids that have a file open
     * \param path File to check
     * \return list of pids that have the file open
     */
    LIBOXIDE_EXPORT QList<int> lsof(const QString& path);
    /*!
     * \brief Get the UID for a username
     * \param name Username to search for
     * \throws std::runtime_error Failed to get the UID for the username
     * \return The UID for the username
     * \snippet examples/oxide.cpp getUID
     */
    LIBOXIDE_EXPORT uid_t getUID(const QString& name);
    /*!
     * \brief Get the GID for a groupname
     * \param name Groupname to search for
     * \throws std::runtime_error Failed to get the GID for the groupname
     * \return The GID for the groupname
     * \snippet examples/oxide.cpp getGID
     */
    LIBOXIDE_EXPORT gid_t getGID(const QString& name);
}
/*! @} */
