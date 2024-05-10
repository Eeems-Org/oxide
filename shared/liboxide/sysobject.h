/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once
// This is required to allow generate_xml.sh to work
#ifndef LIBOXIDE_SYSOBJECT_H
#define LIBOXIDE_SYSOBJECT_H

#include "liboxide_global.h"

#include <string>
#include <QString>

namespace Oxide {
    /*!
     * \brief A class to make interacting with sysfs easier
     *
     * \snippet examples/oxide.cpp SysObject
     */
    class LIBOXIDE_EXPORT SysObject {
    public:
        explicit SysObject(QString path) : m_path(path.toStdString()){}
        /*!
         * \brief The path to the sysfs interface
         * \return The path to the sysfs interface
         */
        std::string path() { return m_path; }
        /*!
         * \brief Does the sysfs interface exist
         * \return Does the sysfs interface exist
         */
        bool exists();
        /*!
         * \brief Does the sysfs interface have a named property
         * \param name The property name
         * \return If the sysfs interface has a named property
         */
        bool hasProperty(const std::string& name);
        /*!
         * \brief Does the sysfs interface have a named directory
         * \param name The directory name
         * \return If the sysfs interface has a named directory
         */
        bool hasDirectory(const std::string& name);
        /*!
         * \brief Get a named property value as a string
         * \param name The property name
         * \return The string value of the named property
         * \retval "0" Unable to open file. This is to ensure that intProperty will not crash
         */
        std::string strProperty(const std::string& name);
        /*!
         * \brief Get a named property value as an int
         * \param name The property name
         * \return The int value of the named property
         */
        int intProperty(const std::string& name);
        /*!
         * \brief Get the path to a named property
         * \param name The property name
         * \return The path to the named property
         */
        std::string propertyPath(const std::string& name);
        /*!
         * \brief Get the contents of uevent for this sysobject
         * \return uevent properties
         */
        QMap<QString, QString> uevent();

    private:
        std::string m_path;
    };
}
#endif // LIBOXIDE_SYSOBJECT_H
/*! @} */
