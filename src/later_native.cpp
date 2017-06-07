#include "callback_registry.h"

extern CallbackRegistry callbackRegistry;

extern "C" void execLaterNative(void (*func)(void*), void* data, double secs) {
  callbackRegistry.add(func, data, secs);
}
