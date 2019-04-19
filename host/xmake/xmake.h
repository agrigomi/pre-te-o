#ifndef __XMAKE_H__
#define __XMAKE_H__

#include "repository.h"

#define HEAP_SIZE	((1024 *1024) * 4)

#ifdef _GCC_
#define TRACE(fmt,...) printf(fmt,__VA_ARGS__);

#ifdef DEBUG
#define DBG(fmt,...) printf(fmt,__VA_ARGS__);
#else
#define DBG(fmt,...)
#endif
#endif // _GCC_

#define XMAKE_VERSION "1.0.0"

void xmake_process(void);

#endif

