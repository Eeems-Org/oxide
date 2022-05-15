#ifndef LIBOXIDE_GLOBAL_H
#define LIBOXIDE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIBOXIDE_LIBRARY)
#  define LIBOXIDE_EXPORT Q_DECL_EXPORT
#else
#  define LIBOXIDE_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBOXIDE_GLOBAL_H
