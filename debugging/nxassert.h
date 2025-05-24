#ifndef _NXASSERT_H_
#define _NXASSERT_H_

#include <assert.h>

/* nxassert only active for NuttX build */

#ifndef DESKTOP_BUILD
#define nxassert(expr) assert(expr)
#define nxfail(msg) assert(0 && msg)
#else
#define nxassert(expr)
#define nxfail(expr)
#endif

#endif // _NXASSERT_H_
