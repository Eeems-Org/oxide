/*!
 * \file json.h
 */
#pragma once

#include "liboxide_global.h"

#include <QVariant>
#include <QJsonArray>
#include <QDBusArgument>

/*!
 * The JSON namespace
 */
namespace Oxide::JSON {
    /*!
     * \brief Decode a DBus Argument into a QVariant
     * \param DBus Argument to decode
     * \return QVariant
     */
    LIBOXIDE_EXPORT QVariant decodeDBusArgument(const QDBusArgument& arg);
    /*!
     * \brief Sanitize a QVariant into a value that can be converted to JSON
     * \param QVariant to sanitize
     * \return Sanitized value
     */
    LIBOXIDE_EXPORT QVariant sanitizeForJson(QVariant value);
    /*!
     * \brief Convert a QVariant to a JSON string
     * \param QVariant to convert
     * \return JSON string
     */
    LIBOXIDE_EXPORT QString toJson(QVariant value);
    /*!
     * \brief Convert a JSON string into a QVariant
     * \param JSON string to convert
     * \return The converted QVaraint
     */
    LIBOXIDE_EXPORT QVariant fromJson(QByteArray json);
    /*!
     * \brief Convert a JSON file into a QVariant
     * \param JSON fle to convert
     * \return The converted QVaraint
     */
    LIBOXIDE_EXPORT QVariant fromJson(QFile* file);
}
