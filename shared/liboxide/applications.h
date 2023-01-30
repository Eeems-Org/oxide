/*!
 * \addtogroup Applications
 * \brief The Applications module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QString>
#include <QFile>
#include <QList>

#include <string>

/*!
 * \brief The applications namespace
 */
namespace Oxide::Applications{
    /*!
    * \typedef ApplicationType
    * \brief Application registration type
    */
    typedef enum {
        Foreground, /*!< Only runs in the foreground */
        Background, /*!< Only runs in the background */
        Backgroundable /*!< Runs in either the foreground or background */
    } ApplicationType;
    /*!
     * \typedef ErrorLevel
     * \brief ValidationError error levels
     */
    typedef enum{
        Hint, /*!< A hint */
        Deprecation, /*!< A deprecation warning */
        Warning, /*!< A warning */
        Error, /*!< An error */
        Critical /*!< A critical error*/
    } ErrorLevel;
    /*!
     * \struct ValidationError
     * \brief Errors returned by validateRegistration
     */
    typedef struct{
        ErrorLevel level; /*!< Error level */
        QString msg; /*!< Error message */
    } ValidationError;
    /*!
     * \brief Convert an application registration to a QVariantMap
     * \param Application registration to convert
     * \param Name of the application
     * \return QVariantMap of the cached application
     */
    LIBOXIDE_EXPORT QVariantMap registrationToMap(const QJsonObject& app, const QString& name = "");
    /*!
     * \brief Convert an ErrorLevel to a human readable string when piping to a text stream
     * \param Text stream to write to
     * \param Error level
     * \return Resulting text stream
     * \sa ErrorLevel
     */
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ErrorLevel& l);
    /*!
     * \brief Convert an ValidationError to a human readable string when piping to a text stream
     * \param Text stream to write to
     * \param Validation error to convert
     * \return Resulting text stream
     * \sa ValidationError
     */
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ValidationError& t);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param Path to the file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const char* path);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param Path to the file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const std::string& path);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param Path to the file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const QString& path);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param The file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(QFile* file);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param The file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const QString& name, const QJsonObject& app);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param Path to the application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const char* path);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param Path to the application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const std::string& path);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param Path to the application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const QString& path);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param The application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(QFile* file);
}
/*! @} */
