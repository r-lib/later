#include "later.h"
#include <Rcpp.h>
#include <queue>

#include "callback_registry.h"

// Declare platform-specific functions that are implemented in
// later_posix.cpp and later_win32.cpp.
void ensureInitialized();
void doExecLater(Rcpp::Function callback, double delaySecs);

// This is just quote(base::sys.nframe()). We create this from R and
// store it, because I don't want to learn how to parse strings into
// call SEXPRs from C/C++.
static SEXP nframes;

// Save a call expression as NFramesCallback. This is called at startup.
// [[Rcpp::export]]
void saveNframesCallback(SEXP exp) {
  // TODO: Is R_PreserveObject necessary here?
  R_PreserveObject(exp);
  
  nframes = exp;
}

// Returns true if sys.nframes() returns 0.
bool at_top_level() {
  int frames = Rcpp::as<int>(Rf_eval(nframes, R_GlobalEnv));
  return frames == 0;
}

// The queue of user-provided callbacks that are scheduled to be
// executed.
CallbackRegistry callbackRegistry;

// [[Rcpp::export]]
bool execCallbacks() {
  std::vector<Callback> callbacks = callbackRegistry.take();
  // if (callbacks.size())
  //   printf("Executing %ld\n", callbacks.size());
  for (std::vector<Callback>::iterator it = callbacks.begin();
    it != callbacks.end();
    it++) {
    // TODO: What to do about errors/warnings that occur here?
    (*it)();
  }
  return !callbacks.empty();
}

bool idle() {
  return callbackRegistry.empty();
}

// [[Rcpp::export]]
void execLater(Rcpp::Function callback, double delaySecs) {
  ensureInitialized();
  doExecLater(callback, delaySecs);
}
