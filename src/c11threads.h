#ifndef _C11_THREADS_H_
#define _C11_THREADS_H_


/* If there's C11 <threads.h> support, use it; otherwise use tinycthread.h,
 * which has a compatible API. */
#if THREADS_H_SUPPORT==1
  #include <threads.h>
#elif THREADS_H_SUPPORT==-1
  #include "tinycthread/tinycthread.h"
#else
  #error THREADS_H_SUPPORT must be set to 1 or -1.
#endif


#endif
