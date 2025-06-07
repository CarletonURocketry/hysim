#ifndef _NXASSERT_H_
#define _NXASSERT_H_

#include <assert.h>

/* nxassert only active for NuttX build */

#ifndef DESKTOP_BUILD

#ifdef CONFIG_DEBUG_ASSERTIONS

/* If debug options enabled, use DEBUGASSERT as underlying assertion mechanism */

#define nxassert(expr) DEBUGASSERT(expr)
#define nxfail(msg) DEBUGASSERT(0 && msg)

#else

/* Use assert as underlying assertion mechanism */

#define nxassert(expr) assert(expr)
#define nxfail(msg) assert(0 && msg)

#endif

/* On desktop, we only want the regular assert statements to do anything */

#else

#define nxassert(expr)
#define nxfail(expr)

#endif

#endif // _NXASSERT_H_
