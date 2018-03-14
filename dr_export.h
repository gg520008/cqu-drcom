
#ifndef DR_EXPORT_H
#define DR_EXPORT_H

#ifdef DR_STATIC_DEFINE
#  define DR_EXPORT
#  define DR_NO_EXPORT
#else
#  ifndef DR_EXPORT
#    ifdef dr_EXPORTS
        /* We are building this library */
#      define DR_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define DR_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef DR_NO_EXPORT
#    define DR_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef DR_DEPRECATED
#  define DR_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef DR_DEPRECATED_EXPORT
#  define DR_DEPRECATED_EXPORT DR_EXPORT DR_DEPRECATED
#endif

#ifndef DR_DEPRECATED_NO_EXPORT
#  define DR_DEPRECATED_NO_EXPORT DR_NO_EXPORT DR_DEPRECATED
#endif

#define DEFINE_NO_DEPRECATED 0
#if DEFINE_NO_DEPRECATED
# define DR_NO_DEPRECATED
#endif

#endif
