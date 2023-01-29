/*!
 * \file applications.h
 */
#pragma once

#include "liboxide_global.h"

#include <QString>
#include <QFile>
#include <QList>

#include <string>

/*!
 * The applications namespace
 */
namespace Oxide::Applications{
    /*!
     * \typedef ErrorLevel
     * \brief ValidationError error levels
     */
    typedef enum{
        Hint, /*!< A hint */
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
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ErrorLevel& l);
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
}
