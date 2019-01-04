#ifndef DEBUG_H
#define DEBUG_H

// See the Makevars file to see how to compile with various debugging settings.

#if defined(DEBUG_THREAD)
#include "tinycthread.h"

extern thrd_t __main_thread__;
extern thrd_t __background_thread__;

// This must be called from the main thread so that thread assertions can be
// tested later.
#define REGISTER_MAIN_THREAD()       __main_thread__ = thrd_current();
#define REGISTER_BACKGROUND_THREAD() __background_thread__ = thrd_current();
#define ASSERT_MAIN_THREAD()         assert(thrd_current() == __main_thread__);
#define ASSERT_BACKGROUND_THREAD()   assert(thrd_current() == __background_thread__);

#else
#define REGISTER_MAIN_THREAD()
#define REGISTER_BACKGROUND_THREAD()
#define ASSERT_MAIN_THREAD()
#define ASSERT_BACKGROUND_THREAD()

#endif // defined(DEBUG_THREAD)


#endif
