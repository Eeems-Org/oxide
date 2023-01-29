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
    typedef enum{
        Hint,
        Warning,
        Error,
        Critical
    } ErrorLevel;
    typedef struct{
        ErrorLevel level;
        QString msg;
    } ValidationError;
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ErrorLevel& l);
    LIBOXIDE_EXPORT QTextStream& operator<<(QTextStream& s, const ValidationError& t);
    /*!
     * \brief Validate a application registration file and return any errors found
     * \param file
     * \return
     */
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const char* path);
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const std::string& path);
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(const QString& path);
    LIBOXIDE_EXPORT QList<ValidationError> validateRegistration(QFile* file);
}
