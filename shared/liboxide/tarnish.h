/*!
 * \addtogroup Oxide::Tarnish
 * \brief The Tarnish module
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "dbus.h"

using namespace codes::eeems::oxide1;

namespace Oxide::Tarnish {
    /*!
     * \brief Get the current General API instance
     * \return The current General API instance
     */
    General* getApi();
    QDBusObjectPath requestAPI(std::string name);
    /*!
     * \brief Connect to the current tarnish instance
     */
    void connect();
    /*!
     * \brief Connect to the current tarnish instance
     * \param Name of current application
     */
    void connect(std::string name);
    /*!
     * \brief Get the tarnish PID
     * \return The tarnish PID
     */
    int tarnishPid();
}
