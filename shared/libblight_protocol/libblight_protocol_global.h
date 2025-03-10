#pragma once

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#  if defined(LIBBLIGHT_PROTOCOL_LIBRARY)
#    define LIBBLIGHT_PROTOCOL_EXPORT __attribute__((visibility("default")))
#  else
#    define LIBBLIGHT_PROTOCOL_EXPORT
#  endif
#else
#  define DEBUG
#  define LIBBLIGHT_PROTOCOL_EXPORT
#endif
#if defined(__clang__) || defined (__GNUC__)
# define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
# define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif
