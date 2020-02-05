#ifndef _LATER_H_
#define _LATER_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include "callback_registry.h"

// This should be kept in sync with LATER_H_API_VERSION in
// inst/include/later.h. Whenever the interface between inst/include/later.h
// and the code in src/ changes, these values should be incremented.
#define LATER_DLL_API_VERSION 2

#define GLOBAL_LOOP 0

boost::shared_ptr<CallbackRegistry> getGlobalLoop();

bool execCallbacksForTopLevel();
bool at_top_level();

bool execCallbacks(double timeoutSecs = 0, bool runAll = true, int loop = GLOBAL_LOOP);
bool idle(int loop);

extern "C" uint64_t execLaterNative(void (*func)(void*), void* data, double secs);
extern "C" uint64_t execLaterNative2(void (*func)(void*), void* data, double secs, int loop);
extern "C" int apiVersion();

#endif // _LATER_H_
