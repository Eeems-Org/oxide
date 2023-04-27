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
     * \brief The event_device class
     */
    class LIBOXIDE_EXPORT event_device {
    public:
        static input_event create_event(ushort type, ushort code, int value);
        /*!
         * \brief event_device
         * \param path
         * \param flags
         */
        event_device(const string& path, int flags);
        ~event_device();
        /*!
         * \brief close
         */
        void close();
        /*!
         * \brief open
         */
        void open();
        /*!
         * \brief lock
         * \return
         */
        int lock();
        /*!
         * \brief unlock
         * \return
         */
        int unlock();
        /*!
         * \brief write
         * \param ie
         */
        void write(input_event ie);
        /*!
         * \brief write_event
         * \param device
         * \param type
         * \param code
         * \param value
         */
        void write(ushort type, ushort code, int value);
        /*!
         * \brief ev_syn
         */
        void ev_syn();
        /*!
         * \brief ev_dropped
         */
        void ev_dropped();
        /*!
         * \brief error
         */
        int error;
        /*!
         * \brief fd
         */
        int fd;
        /*!
         * \brief device
         */
        string device;
        /*!
         * \brief locked
         */
        bool locked = false;
    private:
        int flags;
    };
}
/*! @} */
