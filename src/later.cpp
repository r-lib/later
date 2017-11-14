#include "later.h"
#include <Rcpp.h>
#include <queue>

#include "callback_registry.h"

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
// base::sys.nframe().
int sys_nframe() {
  SEXP e, result;
  int errorOccurred;

  PROTECT(e = Rf_lang1(Rf_install("sys.nframe")));
  result = R_tryEval(e, R_BaseEnv, &errorOccurred);

  if (errorOccurred) {
    UNPROTECT(1);
    return -1;
  }

  int value = INTEGER(result)[0];
  UNPROTECT(1);
  return value;
}

// Returns true if execCallbacks is executing, or sys.nframes() returns 0.
bool at_top_level() {
  if (exec_callbacks_reentrancy_count != 0)
    return false;
  return sys_nframe() == 0;
}

// The queue of user-provided callbacks that are scheduled to be
// executed.
CallbackRegistry callbackRegistry;

// [[Rcpp::export]]
bool execCallbacks(double timeoutSecs) {
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
    std::vector<Callback> callbacks = callbackRegistry.take(1, now);
    if (callbacks.size() == 0) {
      break;
    }
    // This line may throw errors!
    callbacks[0]();
  }
  return true;
}

// [[Rcpp::export]]
bool idle() {
  return callbackRegistry.empty();
}

// [[Rcpp::export]]
void execLater(Rcpp::Function callback, double delaySecs) {
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
