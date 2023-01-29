/*!
 * \file sysobject.h
 */
#pragma once

#include "liboxide_global.h"

#include <string>
#include <QString>

namespace Oxide {
    /*!
     * \brief A class to make interacting with sysfs easier
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
         * \param The property name
         * \return If the sysfs interface has a named property
         */
        bool hasProperty(const std::string& name);
        /*!
         * \brief Does the sysfs interface have a named directory
         * \param The directory name
         * \return If the sysfs interface has a named directory
         */
        bool hasDirectory(const std::string& name);
        /*!
         * \brief Get a named property value as a string
         * \param The property name
         * \return The string value of the named property
         */
        std::string strProperty(const std::string& name);
        /*!
         * \brief Get a named property value as an int
         * \param The property name
         * \return The int value of the named property
         */
        int intProperty(const std::string& name);
        /*!
         * \brief Get the path to a named property
         * \param The property name
         * \return The path to the named property
         */
        std::string propertyPath(const std::string& name);

    private:
        std::string m_path;
    };
}
