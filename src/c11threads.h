#ifndef _C11_THREADS_H_
#define _C11_THREADS_H_


/* If there's C11 <threads.h> support, use it; otherwise use tinycthread.h,
 * which has a compatible API. */
#ifdef THREADS_H_SUPPORT
  #include <threads.h>
#else
  #include "tinycthread/tinycthread.h"
#endif


#endif
