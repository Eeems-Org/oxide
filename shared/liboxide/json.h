/*!
 * \addtogroup JSON
 * \brief The JSON module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QVariant>
#include <QJsonArray>
#include <QDBusArgument>
/*!
 * \brief The JSON namespace
 */
namespace Oxide::JSON {
    /*!
     * \brief Decode a DBus Argument into a QVariant
     * \param arg DBus Argument to decode
     * \return QVariant
     * \retval QVariant() Unable to decode QDBusArgument
     */
    LIBOXIDE_EXPORT QVariant decodeDBusArgument(const QDBusArgument& arg);
    /*!
     * \brief Sanitize a QVariant into a value that can be converted to JSON
     * \param value QVariant to sanitize
     * \return Sanitized value
     */
    LIBOXIDE_EXPORT QVariant sanitizeForJson(QVariant value);
    /*!
     * \brief Convert a QVariant to a JSON string
     * \param value QVariant to convert
     * \param format Format to use
     * \return JSON string
     */
    LIBOXIDE_EXPORT QString toJson(QVariant value, QJsonDocument::JsonFormat format = QJsonDocument::Compact);
    /*!
     * \brief Convert a JSON string into a QVariant
     * \param json JSON string to convert
     * \return The converted QVaraint
     */
    LIBOXIDE_EXPORT QVariant fromJson(QByteArray json);
    /*!
     * \brief Convert a JSON file into a QVariant
     * \param file JSON fle to convert
     * \return The converted QVaraint
     */
    LIBOXIDE_EXPORT QVariant fromJson(QFile* file);
}
/*! @} */
