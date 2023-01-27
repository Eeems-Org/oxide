/*!
 * \file sentry.h
 */
#ifndef OXIDE_SENTRY_H
#define OXIDE_SENTRY_H

#include "liboxide_global.h"

#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <functional>

#ifdef SENTRY
#include <sentry.h>
#include <sentry/src/sentry_tracing.h>
#endif

/*!
 *\brief The Sentry namespace
 */
namespace Oxide::Sentry{
    /*!
     * \brief A sentry_transaction_t wrapper
     */
    struct Transaction {
#ifdef SENTRY
        /*!
         * \brief The sentry_transaction_t instance
         */
        sentry_transaction_t* inner;
        /*!
         * \brief Create a sentry_transaction_t wrapper
         * \param sentry_transaction_t instance to wrap
         */
        explicit Transaction(sentry_transaction_t* t);
#else
        void* inner;
        explicit Transaction(void* t);
#endif
    };
    /*!
     * \brief A sentry_span_t wrapper
     */
    struct Span {
#ifdef SENTRY
        /*!
         * \brief The sentry_span_t instance
         */
        sentry_span_t* inner;
        /*!
         * \brief Create a sentry_span_t wrapper
         * \param The sentry_span_t instance to wrap
         */
        explicit Span(sentry_span_t* s);
#else
        void* inner;
        explicit Span(void* s);
#endif
    };
    /*!
     * \brief Get the boot identifier of the device using sd_id128_get_boot
     * \return The boot identifier
     */
    LIBOXIDE_EXPORT const char* bootId();
    /*!
     * \brief Get the machine identifier of the device using sd_id128_get_machine
     * \return The machine identifier
     */
    LIBOXIDE_EXPORT const char* machineId();
    /*!
     * \brief Initialize sentry tracking
     * \param Name of the application
     * \param Arguments passed to the application
     * \param If automatic session tracking should be enabled
     */
    LIBOXIDE_EXPORT void sentry_init(const char* name, char* argv[], bool autoSessionTracking = true);
    /*!
     * \brief Create a breadcrumb in the current sentry transaction
     * \param Category of the breadcrumb
     * \param Message of the breadcrumb
     * \param Type of breadcrumb
     * \param Logging level of the breadcrumb
     */
    LIBOXIDE_EXPORT void sentry_breadcrumb(const char* category, const char* message, const char* type = "default", const char* level = "info");
    /*!
     * \brief Start a transaction
     * \param Name of the transaction
     * \param Action being performed
     * \return The transaction wrapper
     */
    LIBOXIDE_EXPORT Transaction* start_transaction(const std::string& name, const std::string& action);
    /*!
     * \brief Stop a sentry transaction
     * \param The transaction wrapper to stop
     */
    LIBOXIDE_EXPORT void stop_transaction(Transaction* transaction);
    /*!
     * \brief Record a sentry trancation
     * \param Name of the transaction
     * \param Action being performed
     * \param Code to run inside the transaction
     */
    LIBOXIDE_EXPORT void sentry_transaction(const std::string& name, const std::string& action, std::function<void(Transaction* transaction)> callback);
    /*!
     * \brief Start a span inside a sentry transaction
     * \param Transaction wrapper to attach the span to
     * \param Operation being performed
     * \param Description of the span
     * \return The span wrapper
     */
    LIBOXIDE_EXPORT Span* start_span(Transaction* transaction, const std::string& operation, const std::string& description);
    /*!
     * \brief Start a span inside another sentry span
     * \param The parent sentry span wrapper
     * \param Operation being performed
     * \param Description of the span
     * \return The span wrapper
     */
    LIBOXIDE_EXPORT Span* start_span(Span* parent, const std::string& operation, const std::string& description);
    /*!
     * \brief Stop a sentry span
     * \param The span wrapper to stop
     */
    LIBOXIDE_EXPORT void stop_span(Span* span);
    /*!
     * \brief Record a sentry span
     * \param The transaction wrapper to attach this span to
     * \param Operation being performed
     * \param Description of the span
     * \param Code to run inside the transaction
     */
    LIBOXIDE_EXPORT void sentry_span(Transaction* transaction, const std::string& operation, const std::string& description, std::function<void()> callback);
    /*!
     * \brief Record a sentry span
     * \param The transaction wrapper to attach this span to
     * \param Operation being performed
     * \param Description of the span
     * \param Code to run inside the transaction
     */
    LIBOXIDE_EXPORT void sentry_span(Transaction* transaction, const std::string& operation, const std::string& description, std::function<void(Span* span)> callback);
    /*!
     * \brief Record a sentry span
     * \param The span wrapper to attach this span to
     * \param Operation being performed
     * \param Description of the span
     * \param Code to run inside the transaction
     */
    LIBOXIDE_EXPORT void sentry_span(Span* parent, const std::string& operation, const std::string& description, std::function<void()> callback);
    /*!
     * \brief Record a sentry span
     * \param The span wrapper to attach this span to
     * \param Operation being performed
     * \param Description of the span
     * \param Code to run inside the transaction
     */
    LIBOXIDE_EXPORT void sentry_span(Span* parent, const std::string& operation, const std::string& description, std::function<void(Span* span)> callback);
    /*!
     * \brief Trigger a crash. Useful to test that sentry integration is working
     */
    LIBOXIDE_EXPORT void trigger_crash();
}

#endif // OXIDE_SENTRY_H
