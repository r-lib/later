#include "later.h"
#include <Rcpp.h>
#include <map>
#include <queue>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "debug.h"
#include "utils.h"

#include "callback_registry.h"
#include "interrupt.h"

// For debug.h
#if defined(DEBUG_THREAD)
thrd_t __main_thread__;
#endif

// Declare platform-specific functions that are implemented in
// later_posix.cpp and later_win32.cpp.
// [[Rcpp::export]]
void ensureInitialized();
uint64_t doExecLater(boost::shared_ptr<CallbackRegistry> callbackRegistry, Rcpp::Function callback, double delaySecs, bool resetTimer);
uint64_t doExecLater(boost::shared_ptr<CallbackRegistry> callbackRegistry, void (*callback)(void*), void* data, double delaySecs, bool resetTimer);

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

  BEGIN_SUSPEND_INTERRUPTS {
    PROTECT(e = Rf_lang1(Rf_install("sys.nframe")));
    PROTECT(result = R_tryEval(e, R_BaseEnv, &errorOccurred));

    if (errorOccurred) {
      value = -1;
    } else {
      value = INTEGER(result)[0];
    }

    UNPROTECT(2);
  } END_SUSPEND_INTERRUPTS;

  return value;
}

// Returns true if execCallbacks is executing, or sys.nframes() returns 0.
bool at_top_level() {
  ASSERT_MAIN_THREAD()
  if (exec_callbacks_reentrancy_count != 0)
    return false;

  int nframe = sys_nframe();
  if (nframe == -1) {
    throw Rcpp::exception("Error occurred while calling sys.nframe()");
  }
  return nframe == 0;
}


// Each callback registry represents one event loop. Note that traversing and
// modifying callbackRegistries should always happens on the same thread, to
// avoid races.
std::map<int, boost::shared_ptr<CallbackRegistry> > callbackRegistries;

// [[Rcpp::export]]
bool existsCallbackRegistry(int loop) {
  ASSERT_MAIN_THREAD()
  return (callbackRegistries.find(loop) != callbackRegistries.end());
}

// [[Rcpp::export]]
bool createCallbackRegistry(int loop) {
  ASSERT_MAIN_THREAD()
  if (existsCallbackRegistry(loop)) {
    Rcpp::stop("Can't create event loop %d because it already exists.", loop);
  }
  callbackRegistries[loop] = boost::make_shared<CallbackRegistry>();
  return true;
}

// Gets a callback registry by ID. If registry doesn't exist, it will be created.
boost::shared_ptr<CallbackRegistry> getCallbackRegistry(int loop) {
  ASSERT_MAIN_THREAD()
  // TODO: clean up
  if (!existsCallbackRegistry(loop)) {
    Rcpp::stop("Event loop %d does not exist.", loop);
  }
  return callbackRegistries[loop];
}

// [[Rcpp::export]]
bool deleteCallbackRegistry(int loop) {
  ASSERT_MAIN_THREAD()
  if (!existsCallbackRegistry(loop)) {
    return false;
  }

  int n = callbackRegistries.erase(loop);
  
  if (n == 0) return false;
  else return true;
}

// [[Rcpp::export]]
Rcpp::List list_queue_(int loop) {
  ASSERT_MAIN_THREAD()
  return getCallbackRegistry(loop)->list();
}


// [[Rcpp::export]]
bool execCallbacks(double timeoutSecs, bool runAll, int loop) {
  ASSERT_MAIN_THREAD()
  // execCallbacks can be called directly from C code, and the callbacks may
  // include Rcpp code. (Should we also call wrap?)
  Rcpp::RNGScope rngscope;
  ProtectCallbacks pcscope;

  if (!getCallbackRegistry(loop)->wait(timeoutSecs)) {
    return false;
  }
  
  Timestamp now;
  
  do {
    // We only take one at a time, because we don't want to lose callbacks if 
    // one of the callbacks throws an error
    std::vector<Callback_sp> callbacks = getCallbackRegistry(loop)->take(1, now);
    if (callbacks.size() == 0) {
      break;
    }
    // This line may throw errors!
    (*callbacks[0])();
  } while (runAll);
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
    if (!execCallbacks(0, GLOBAL_LOOP))
      return any;
    any = true;
  }
  return any;
}

// [[Rcpp::export]]
bool idle(int loop) {
  ASSERT_MAIN_THREAD()
  return getCallbackRegistry(loop)->empty();
}

// [[Rcpp::export]]
std::string execLater(Rcpp::Function callback, double delaySecs, int loop) {
  ASSERT_MAIN_THREAD()
  ensureInitialized();
  uint64_t callback_id = doExecLater(getCallbackRegistry(loop), callback, delaySecs, loop == GLOBAL_LOOP);

  // We have to convert it to a string in order to maintain 64-bit precision,
  // since R doesn't support 64 bit integers.
  return toString(callback_id);
}



bool cancel(uint64_t callback_id, int loop) {
  if (!existsCallbackRegistry(loop))
    return false;

  boost::shared_ptr<CallbackRegistry> reg = getCallbackRegistry(loop);
  if (!reg)
    return false;

  return reg->cancel(callback_id);
}

// [[Rcpp::export]]
bool cancel(std::string callback_id_s, int loop) {
  uint64_t callback_id;
  std::istringstream iss(callback_id_s);
  iss >> callback_id;

  // If the input is good (just a number with no other text) then eof will be
  // 1 and fail will be 0.
  if (! (iss.eof() && !iss.fail())) {
    return false;
  }

  return cancel(callback_id, loop);
}



// [[Rcpp::export]]
double nextOpSecs(int loop) {
  ASSERT_MAIN_THREAD()
  Optional<Timestamp> nextTime = getCallbackRegistry(loop)->nextTimestamp();
  if (!nextTime.has_value()) {
    return R_PosInf;
  } else {
    Timestamp now;
    return nextTime->diff_secs(now);
  }
}

extern "C" void execLaterNative(void (*func)(void*), void* data, double delaySecs) {
  ensureInitialized();
  int loop = GLOBAL_LOOP;
  doExecLater(getCallbackRegistry(GLOBAL_LOOP), func, data, delaySecs, loop == GLOBAL_LOOP);
}
