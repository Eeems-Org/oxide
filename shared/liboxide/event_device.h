/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QDebug>

#include <string>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

using namespace std;
namespace Oxide {
    /*!
     * \brief A class to simplify managing a /dev/event* file
     */
    class LIBOXIDE_EXPORT event_device {
    public:
        /*!
         * \brief Create an input_event
         * \param type Input event type
         * \param code Input event code
         * \param value Input event value
         * \return
         */
        static input_event create_event(ushort type, ushort code, int value);
        /*!
         * \brief Instantiate a new instance
         * \param path Path to the event device
         * \param flags Flags to use when opening the event device
         */
        event_device(const string& path, int flags);
        /*!
         * \brief Create a copy of an event_device instance
         * \param other Instance to copy
         */
        event_device(const event_device& other);
        ~event_device();
        /*!
         * \brief Close the event device
         * \sa open()
         */
        void close();
        /*!
         * \brief Open the event device
         * \sa fd, error, close()
         */
        void open();
        /*!
         * \brief Grab all input from this device
         * \return If grabbing was successful
         * \retval 0 Successfully grabbed all input
         * \retval EBUSY The event device is grabbed by another process
         * \sa locked unlock()
         */
        int lock();
        /*!
         * \brief Ungrab all input from this device
         * \return If ungrabbing was successful
         * \retval 0 Successfully ungrabbed all input
         * \sa locked, lock()
         */
        int unlock();
        /*!
         * \brief Write an input event to the event device
         * \param ie Input event to write
         * \sa write(ushort, ushort, int)
         */
        void write(input_event ie);
        /*!
         * \brief Write an input event to the event device
         * \param type Input event type
         * \param code Input event code
         * \param value Input event value
         * \sa write(input_event)
         */
        void write(ushort type, ushort code, int value);
        /*!
         * \brief Write an EV_SYN SYN_REPORT event to the event device
         */
        void ev_syn();
        /*!
         * \brief Write an EV_SYN SYN_DROPPED event to the event device
         */
        void ev_dropped();
        /*!
         * \brief Errno reported when opening the event device
         * \sa open()
         */
        int error;
        /*!
         * \brief File descriptor returned when opening the event device
         * \sa open(), close()
         */
        int fd;
        /*!
         * \brief Path to the event device
         * \sa event_device(const string&, int)
         */
        string device;
        /*!
         * \brief If input is currently grabbed
         * \sa lock(), unlock()
         */
        bool locked = false;

    private:
        int flags;
    };
}
/*! @} */
