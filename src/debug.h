#ifndef DEBUG_H
#define DEBUG_H

// See the Makevars file to see how to compile with various debugging settings.

#if defined(DEBUG_THREAD)
#include "tinycthread.h"

extern tct_thrd_t __main_thread__;
extern tct_thrd_t __background_thread__;

// This must be called from the main thread so that thread assertions can be
// tested later.
#define REGISTER_MAIN_THREAD()       __main_thread__ = tct_thrd_current();
#define REGISTER_BACKGROUND_THREAD() __background_thread__ = tct_thrd_current();
#define ASSERT_MAIN_THREAD()         assert(tct_thrd_current() == __main_thread__);
#define ASSERT_BACKGROUND_THREAD()   assert(tct_thrd_current() == __background_thread__);

#else
#define REGISTER_MAIN_THREAD()
#define REGISTER_BACKGROUND_THREAD()
#define ASSERT_MAIN_THREAD()
#define ASSERT_BACKGROUND_THREAD()

#endif // defined(DEBUG_THREAD)


// ============================================================================
// Logging
// ============================================================================

void err_printf(const char *fmt, ...);

enum LogLevel {
  OFF,
  ERROR,
  WARN,
  INFO,
  DEBUG
};

extern LogLevel log_level_;

// This is a macro instead of a function, so that if msg is an expression that
// involves constructing a string, the string construction does not need to be
// executed when the message is not being logged. If it were a function, the
// expression would need to be executed even when the message is not actually
// logged.
//
// Conversion to std::string is done so that msg can be a char* or a
// std::string. This method is needed because macros can't be overloaded.
#define DEBUG_LOG(msg, level) if (log_level_ >= level) err_printf("%s\n", std::string(msg).c_str());

#endif
