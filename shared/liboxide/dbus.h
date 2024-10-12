/*!
 * \addtogroup DBus
 * \brief The DBus module
 *
 * Contains classes [automatically generated DBus interface classes](https://github.com/Eeems-Org/oxide/tree/master/interfaces)
 * @{
 * \file
 */
#pragma once
// This must be here to make precompiled headers happy
#ifndef LIBOXIDE_DBUS_H
#define LIBOXIDE_DBUS_H
#ifdef IN_DOXYGEN
/*!
 * \brief System service DBus interfaces
 */
namespace codes::eeems::oxide1{
    /*!
     * \brief General API interface
     */
    class General : public QDBusAbstractInterface{};
    /*!
     * \brief The Power class
     */
    class Power : public QDBusAbstractInterface{};
    /*!
     * \brief The Wifi class
     */
    class Wifi : public QDBusAbstractInterface{};
    /*!
     * \brief The Network class
     */
    class Network : public QDBusAbstractInterface{};
    /*!
     * \brief The BSS class
     */
    class BSS : public QDBusAbstractInterface{};
    /*!
     * \brief The Apps class
     */
    class Apps : public QDBusAbstractInterface{};
    /*!
     * \brief The Application class
     */
    class Application : public QDBusAbstractInterface{};
    /*!
     * \brief The System class
     */
    class System : public QDBusAbstractInterface{};
    /*!
     * \brief The Screen class
     */
    class Screen : public QDBusAbstractInterface{};
    /*!
     * \brief The Screenshot class
     */
    class Screenshot : public QDBusAbstractInterface{};
    /*!
     * \brief The Notifications class
     */
    class Notifications : public QDBusAbstractInterface{};
    /*!
     * \brief The Notification class
     */
    class Notification : public QDBusAbstractInterface{};
}
#endif
#include "dbusservice_interface.h"
#include "powerapi_interface.h"
#include "wifiapi_interface.h"
#include "network_interface.h"
#include "bss_interface.h"
#include "appsapi_interface.h"
#include "application_interface.h"
#include "systemapi_interface.h"
#include "screenapi_interface.h"
#include "screenshot_interface.h"
#include "notificationapi_interface.h"
#include "notification_interface.h"
#ifdef IN_DOXYGEN
/*!
 * \brief Display server DBus interfaces
 */
namespace codes::eeems::blight1{
    /*!
     * \brief Display server compositor API
     */
    class Compositor : public QDBusAbstractInterface{};
}
#endif
#include "blight_interface.h"
#endif // LIBOXIDE_DBUS_H
/*! @} */
