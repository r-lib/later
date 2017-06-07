#include <dlfcn.h>

void execLaterNative(void (*func)(void*), void* data, double secs) {
  void* hLib = dlopen("/Library/Frameworks/R.framework/Versions/3.4/Resources/library/later/libs/", RTLD_LAZY);
  typedef void (*elnfun)(void (*func)(void*), void*, double);
  elnfun eln = (elnfun)dlsym(hLib, "execLaterNative");
  (*eln)(func, data, secs);
}
