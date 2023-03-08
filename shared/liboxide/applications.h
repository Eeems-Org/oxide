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
 * \def OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY
 * \brief Application directory location
 */
#define OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY "/opt/usr/share/applications"
#define OXIDE_ICONS_DIRECTORY "/opt/usr/share/icons"

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
     * \param app Application registration to convert
     * \param name Name of the application
     * \return QVariantMap of the cached application
     */
    LIBOXIDE_EXPORT QVariantMap registrationToMap(const QJsonObject& app, const QString& name = "");
    /*!
     * \brief Convert an ErrorLevel to a human readable string when piping to a text stream
     * \param s Text stream to write to
     * \param l Error level
     * \return Resulting text stream
     * \sa ErrorLevel
     */
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ErrorLevel& l);
    /*!
     * \brief Convert an ValidationError to a human readable string when piping to a text stream
     * \param s Text stream to write to
     * \param t Validation error to convert
     * \return Resulting text stream
     * \sa ValidationError
     */
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ValidationError& t);
    /*!
     * \brief Convert an ErrorLevel to a human readable string when piping to a text stream
     * \param s Text stream to write to
     * \param l Error level
     * \return Resulting text stream
     * \sa ErrorLevel
     */
    LIBOXIDE_EXPORT QDebug& operator<<(QDebug& s, const ErrorLevel& l);
    /*!
     * \brief Convert an ValidationError to a human readable string when piping to a text stream
     * \param s Text stream to write to
     * \param t Validation error to convert
     * \return Resulting text stream
     * \sa ValidationError
     */
    LIBOXIDE_EXPORT QDebug& operator<<(QDebug& s, const ValidationError& t);
    /*!
     * \brief Compare two ValidationError instances
     * \param v1 First instance to comprae
     * \param v2 Second instance to compare
     * \return If they are the same
     * \sa ValidationError
     */
    LIBOXIDE_EXPORT bool operator==(const ValidationError& v1, const ValidationError& v2);
    /*!
     * \brief Get an application registration.
     * \param path Path to application registration
     * \return Application registration
     */
    LIBOXIDE_EXPORT QJsonObject getRegistration(const char* path);
    /*!
     * \brief Get an application registration.
     * \param path Path to application registration
     * \return Application registration
     */
    LIBOXIDE_EXPORT QJsonObject getRegistration(const std::string& path);
    /*!
     * \brief Get an application registration.
     * \param path Path to application registration
     * \return Application registration
     */
    LIBOXIDE_EXPORT QJsonObject getRegistration(const QString& path);
    /*!
     * \brief Get an application registration.
     * \param file Application registration file
     * \return Application registration
     */
    LIBOXIDE_EXPORT QJsonObject getRegistration(QFile* file);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param path Path to the file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const char* path);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param path Path to the file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const std::string& path);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param path Path to the file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const QString& path);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param file The file to validate
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(QFile* file);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param name Name of the application
     * \param app Application registration file
     * \return List of validation errors
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const QString& name, const QJsonObject& app);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param path Path to the application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const char* path);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param path Path to the application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const std::string& path);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param path Path to the application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const QString& path);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param file The application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(QFile* file);
    /*!
     * \brief Add an application to the tarnish application cache
     * \param name The name of the application
     * \param app The application registration file
     * \return If the application was successfully added
     */
    LIBOXIDE_EXPORT bool addToTarnishCache(const QString& name, const QJsonObject& app);
    /*!
     * \brief Get the path to the directory that an icon would be stored in.
     * \param size Size of the icon.
     * \param theme Icon theme. Default is hicolor.
     * \param context Icon context. Default is apps.
     * \return Path to the directory that would contain the icon.
     */
    LIBOXIDE_EXPORT QString iconDirPath(int size, const QString& theme = "hicolor", const QString& context = "apps");
    /*!
     * \brief Get the path to an icon.
     * \param name Name of the icon.
     * \param size Size of the icon.
     * \param theme Icon theme. Default is hicolor.
     * \param context Icon context. Default is apps.
     * \return Path to the icon.
     */
    LIBOXIDE_EXPORT QString iconPath(const QString& name, int size, const QString& theme = "hicolor", const QString& context = "apps");
    /*!
     * \brief Get the path to an icon from an icon name spec.
     * \param spec Icon name spec using the following format: [theme:][context:]{name}-{size}. e.g. oxide:splash:xochitl-702
     * \return Path to the icon.
     * \retval "" Failed to parse the spec
     */
    LIBOXIDE_EXPORT QString iconPath(const QString& spec);
}
/*! @} */
