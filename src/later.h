#ifndef _LATER_H_
#define _LATER_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include "callback_registry.h"

#define GLOBAL_LOOP 0

boost::shared_ptr<CallbackRegistry> getCallbackRegistry(int loop);

bool execCallbacksForTopLevel();
bool at_top_level();

bool execCallbacks(double timeoutSecs = 0, bool runAll = true, int loop = GLOBAL_LOOP);
bool idle(int loop);

extern "C" void execLaterNative(void (*func)(void*), void* data, double secs);
extern "C" void execLaterNativeLoop(void (*func)(void*), void* data, double secs, int loop);

#endif // _LATER_H_
