#include "later.h"
#include <Rcpp.h>
#include <queue>
#include "debug.h"

#include "callback_registry.h"

// For debug.h
#if defined(DEBUG_THREAD)
thrd_t __main_thread__;
#endif

// Declare platform-specific functions that are implemented in
// later_posix.cpp and later_win32.cpp.
// [[Rcpp::export]]
void ensureInitialized();
void doExecLater(Rcpp::Function callback, double delaySecs);
void doExecLater(void (*callback)(void*), void* data, double delaySecs);

static size_t exec_callbacks_reentrancy_count = 0;

class ProtectCallbacks {
public:
  ProtectCallbacks() {
    exec_callbacks_reentrancy_count++;
  }
  ~ProtectCallbacks() {
    exec_callbacks_reentrancy_count--;
  }
};

// Returns number of frames on the call stack. Basically just a wrapper for
// base::sys.nframe(). Note that this can report that an error occurred if the
// user sends an interrupt while the `sys.nframe()` function is running. I
// believe that the only reason that it should set errorOccurred is because of
// a user interrupt.
int sys_nframe() {
  ASSERT_MAIN_THREAD()
  SEXP e, result;
  int errorOccurred, value;

  PROTECT(e = Rf_lang1(Rf_install("sys.nframe")));
  PROTECT(result = R_tryEval(e, R_BaseEnv, &errorOccurred));

  if (errorOccurred) {
    value = -1;
  } else {
    value = INTEGER(result)[0];
  }

  UNPROTECT(2);
  return value;
}

// Returns true if execCallbacks is executing, or sys.nframes() returns 0.
bool at_top_level() {
  ASSERT_MAIN_THREAD()
  if (exec_callbacks_reentrancy_count != 0)
    return false;

  int failcount = 0;
  int nframe = -1;

  // Try running sys.nframe() up to 10 times. A user interrupt could cause it
  // to report an error even though its not a "real" error.
  // https://github.com/r-lib/later/issues/57
  while (nframe == -1 && failcount < 10) {
    nframe = sys_nframe();
    if (nframe == -1)
      failcount++;
  }
  if (nframe == -1) {
    throw Rcpp::exception("Error occurred while calling sys.nframe()");
  }
  return nframe == 0;
}

// The queue of user-provided callbacks that are scheduled to be
// executed.
CallbackRegistry callbackRegistry;

// [[Rcpp::export]]
bool execCallbacks(double timeoutSecs) {
  ASSERT_MAIN_THREAD()
  // execCallbacks can be called directly from C code, and the callbacks may
  // include Rcpp code. (Should we also call wrap?)
  Rcpp::RNGScope rngscope;
  ProtectCallbacks pcscope;
  
  if (!callbackRegistry.wait(timeoutSecs)) {
    return false;
  }
  
  Timestamp now;
  
  while (true) {
    // We only take one at a time, because we don't want to lose callbacks if 
    // one of the callbacks throws an error
    std::vector<Callback_sp> callbacks = callbackRegistry.take(1, now);
    if (callbacks.size() == 0) {
      break;
    }
    // This line may throw errors!
    (*callbacks[0])();
  }
  return true;
}

// Invoke execCallbacks up to 20 times. At the first iteration where no work is
// done, terminate. We call this from the top level instead of just calling
// execCallbacks because the top level only gets called occasionally (every 10's
// of ms), so tasks that generate other tasks will execute surprisingly slowly.
//
// Example:
// promise_map(1:1000, function(i) {
//   message(i)
//   promise_resolve(i)
// })
bool execCallbacksForTopLevel() {
  bool any = false;
  for (size_t i = 0; i < 20; i++) {
    if (!execCallbacks())
      return any;
    any = true;
  }
  return any;
}

// [[Rcpp::export]]
bool idle() {
  ASSERT_MAIN_THREAD()
  return callbackRegistry.empty();
}

// [[Rcpp::export]]
void execLater(Rcpp::Function callback, double delaySecs) {
  ASSERT_MAIN_THREAD()
  ensureInitialized();
  doExecLater(callback, delaySecs);
}


//' Relative time to next scheduled operation
//'
//' Returns the duration between now and the earliest operation that is currently
//' scheduled, in seconds. If the operation is in the past, the value will be
//' negative. If no operation is currently scheduled, the value will be `Inf`.
//'
//' @export
// [[Rcpp::export]]
double next_op_secs() {
  ASSERT_MAIN_THREAD()
  Optional<Timestamp> nextTime = callbackRegistry.nextTimestamp();
  if (!nextTime.has_value()) {
    return R_PosInf;
  } else {
    Timestamp now;
    return nextTime->diff_secs(now);
  }
}

extern "C" void execLaterNative(void (*func)(void*), void* data, double delaySecs) {
  ensureInitialized();
  doExecLater(func, data, delaySecs);
}
