#ifndef _HYSIM_LOGGING_H_
#define _HYSIM_LOGGING_H_

#ifndef DESKTOP_BUILD
#include <debug.h>
#else
#include <stdio.h>
#endif

#define xstr(a) str(a)
#define str(a) #a
#define __HLOGSTR(fstring) "%s::" fstring

/* Syslog wrappers for hysim */

#ifndef DESKTOP_BUILD

#ifdef CONFIG_HYSIM_PAD_SERVER_LOG_ERR
#define herr(fstring, ...) syslog(LOG_ERR, "ERR:"__HLOGSTR(fstring), __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define herr(fstring, ...)
#endif

#ifdef CONFIG_HYSIM_PAD_SERVER_LOG_WARN
#define hwarn(fstring, ...) syslog(LOG_ERR, "WARN:"__HLOGSTR(fstring), __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define hwarn(fstring, ...)
#endif

#ifdef CONFIG_HYSIM_PAD_SERVER_LOG_INFO
#define hinfo(fstring, ...) syslog(LOG_ERR, "INFO:"__HLOGSTR(fstring), __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define hinfo(fstring, ...)
#endif

#else

/* Desktop build uses stdio implementation */

#define herr(fstring, ...) fprintf(stderr, "ERR:"__HLOGSTR(fstring), __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#define hwarn(fstring, ...) fprintf(stderr, "WARN:"__HLOGSTR(fstring), __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#define hinfo(fstring, ...) fprintf(stdout, "INFO:"__HLOGSTR(fstring), __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)

#endif

#endif // _HYSIM_LOGGING_H_
