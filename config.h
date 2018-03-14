#ifndef DR_CONFIG_H
#define DR_CONFIG_H

/* #undef USE_STDCALL */

#ifdef _WIN32

#ifdef USE_STDCALL

#define DR_CALL_CONV __stdcall

#else

#define DR_CALL_CONV

#endif

#else

#define DR_CALL_CONV

#endif

#endif
