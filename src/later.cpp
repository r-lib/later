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

// This is just quote(base::sys.nframe()). We create this from R and
// store it, because I don't want to learn how to parse strings into
// call SEXPRs from C/C++.
static SEXP nframes;
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

// Save a call expression as NFramesCallback. This is called at startup.
// [[Rcpp::export]]
void saveNframesCallback(SEXP exp) {
  // TODO: Is R_PreserveObject necessary here?
  R_PreserveObject(exp);
  
  nframes = exp;
}

// Returns true if execCallbacks is executing, or sys.nframes() returns 0.
bool at_top_level() {
  if (exec_callbacks_reentrancy_count != 0)
    return false;
  int frames = Rcpp::as<int>(Rf_eval(nframes, R_GlobalEnv));
  return frames == 0;
}

// The queue of user-provided callbacks that are scheduled to be
// executed.
CallbackRegistry callbackRegistry;

// [[Rcpp::export]]
bool execCallbacks() {
  // execCallbacks can be called directly from C code, and the callbacks may
  // include Rcpp code. (Should we also call wrap?)
  Rcpp::RNGScope rngscope;
  ProtectCallbacks pcscope;
  
  bool any = false;
  while (true) {
    // We only take one at a time, because we don't want to lose callbacks if 
    // one of the callbacks throws an error
    std::vector<Callback> callbacks = callbackRegistry.take(1);
    if (callbacks.size() == 0) {
      break;
    }
    any = true;
    // This line may throw errors!
    callbacks[0]();
  }
  return any;
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

// [[Rcpp::export]]
double secsToNext() {
  Optional<Timestamp> nextTime = callbackRegistry.nextTimestamp();
  if (!nextTime.has_value()) {
    return R_PosInf;
  } else {
    Timestamp now;
    return (*nextTime).diff_secs(now);
  }
}

extern "C" void execLaterNative(void (*func)(void*), void* data, double delaySecs) {
  ensureInitialized();
  doExecLater(func, data, delaySecs);
}
