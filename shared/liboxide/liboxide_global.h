/*!
 * \file liboxide_global.h
 */
#ifndef LIBOXIDE_GLOBAL_H
#define LIBOXIDE_GLOBAL_H


#include <QtCore/qglobal.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#  if defined(LIBOXIDE_LIBRARY)
#    define LIBOXIDE_EXPORT Q_DECL_EXPORT
#  else
#    define LIBOXIDE_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define SENTRY
#  define DEBUG
#  define LIBOXIDE_EXPORT
#endif

#endif // LIBOXIDE_GLOBAL_H
