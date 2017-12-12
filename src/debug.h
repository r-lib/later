#ifndef DEBUG_H
#define DEBUG_H

// See the Makevars file to see how to compile with various debugging settings.

#if defined(DEBUG_THREAD) && !defined(_WIN32)
#include <pthread.h>
#include <assert.h>

extern pthread_t __main_thread__;
extern pthread_t __background_thread__;

// This must be called from the main thread so that thread assertions can be
// tested later.
#define REGISTER_MAIN_THREAD()       __main_thread__ = pthread_self();
#define REGISTER_BACKGROUND_THREAD() __background_thread__ = pthread_self();
#define ASSERT_MAIN_THREAD()         assert(pthread_self() == __main_thread__);
#define ASSERT_BACKGROUND_THREAD()   assert(pthread_self() == __background_thread__);

#else
#define REGISTER_MAIN_THREAD()
#define REGISTER_BACKGROUND_THREAD()
#define ASSERT_MAIN_THREAD()
#define ASSERT_BACKGROUND_THREAD()

#endif // defined(DEBUG_THREAD) && !defined(_WIN32)


#endif
