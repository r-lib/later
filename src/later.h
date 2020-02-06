#ifndef _LATER_H_
#define _LATER_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include "callback_registry.h"

// This should be kept in sync with LATER_H_API_VERSION in
// inst/include/later.h. Whenever the interface between inst/include/later.h
// and the code in src/ changes, these values should be incremented.
#define LATER_DLL_API_VERSION 2

#define GLOBAL_LOOP_ID 0

boost::shared_ptr<CallbackRegistry> getGlobalRegistry();

bool execCallbacksForTopLevel();
bool at_top_level();

bool execCallbacks(double timeoutSecs, bool runAll, SEXP loop_xptr);
bool idle(int loop);

extern "C" uint64_t execLaterNative(void (*func)(void*), void* data, double secs);
extern "C" uint64_t execLaterNative2(void (*func)(void*), void* data, double secs, int loop_id);
extern "C" int apiVersion();

void ensureInitialized();
// Declare platform-specific functions that are implemented in later_posix.cpp
// and later_win32.cpp.
void ensureAutorunnerInitialized();

#endif // _LATER_H_
