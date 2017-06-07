bool execCallbacks();
bool at_top_level();
bool idle();
extern "C" void execLaterNative(void (*func)(void*), void* data, double secs);
