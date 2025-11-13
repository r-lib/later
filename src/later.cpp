#include "later.h"
#include <Rcpp.h>
#include <queue>
#include <memory>
#include "debug.h"
#include "utils.h"
#include "threadutils.h"

#include "callback_registry.h"
#include "callback_registry_table.h"

#include "interrupt.h"

using std::shared_ptr;

static size_t exec_callbacks_reentrancy_count = 0;

// instance has global scope as declared in callback_registry_table.h
CallbackRegistryTable callbackRegistryTable;


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

// ============================================================================
// Current registry/event loop
// ============================================================================
//
// In the R code, the term "loop" is used. In the C++ code, the terms "loop"
// and "registry" are both used. "Loop" is usually used when interfacing with
// R-facing event loop, and "registry" is usually used when interfacing with
// the implementation, which uses a callback registry.
//
// The current registry is kept track of entirely in C++, and not in R
// (although it can be queried from R). This is because when running a loop
// with children, it sets the current loop as it runs each of the children,
// and to do so in R would require calling back into R for each child, which
// would impose more overhead.

static int current_registry;

// [[Rcpp::export(rng = false)]]
void setCurrentRegistryId(int id) {
  ASSERT_MAIN_THREAD()
  current_registry = id;
}

// [[Rcpp::export(rng = false)]]
int getCurrentRegistryId() {
  ASSERT_MAIN_THREAD()
  return current_registry;
}

// Class for setting current registry and resetting when function exits, using
// RAII.
class CurrentRegistryGuard {
public:
  CurrentRegistryGuard(int id) {
    ASSERT_MAIN_THREAD()
    old_id = getCurrentRegistryId();
    setCurrentRegistryId(id);
  }
  ~CurrentRegistryGuard() {
    setCurrentRegistryId(old_id);
  }
private:
  int old_id;
};


// ============================================================================
// Callback registry functions
// ============================================================================

shared_ptr<CallbackRegistry> getGlobalRegistry() {
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(GLOBAL_LOOP);
  if (registry == nullptr) {
    Rf_error("Global registry does not exist.");
  }
  return registry;
}

// This deletes a CallbackRegistry and deregisters it as a child of its
// parent. Any children of this registry are orphaned -- they no longer have a
// parent. (Maybe this should be an option?)
//
// [[Rcpp::export(rng = false)]]
bool deleteCallbackRegistry(int loop_id) {
  ASSERT_MAIN_THREAD()
  if (loop_id == GLOBAL_LOOP) {
    Rcpp::stop("Can't destroy global loop.");
  }
  if (loop_id == getCurrentRegistryId()) {
    Rcpp::stop("Can't destroy current loop.");
  }

  return callbackRegistryTable.remove(loop_id);
}


// This is called when the R loop handle is GC'd.
// [[Rcpp::export(rng = false)]]
bool notifyRRefDeleted(int loop_id) {
  ASSERT_MAIN_THREAD()
  if (loop_id == GLOBAL_LOOP) {
    Rcpp::stop("Can't notify that reference to global loop is deleted.");
  }
  if (loop_id == getCurrentRegistryId()) {
    Rcpp::stop("Can't notify that reference to current loop is deleted.");
  }

  return callbackRegistryTable.notifyRRefDeleted(loop_id);
}


// [[Rcpp::export(rng = false)]]
void createCallbackRegistry(int id, int parent_id) {
  ASSERT_MAIN_THREAD()
  callbackRegistryTable.create(id, parent_id);
}

// [[Rcpp::export(rng = false)]]
bool existsCallbackRegistry(int id) {
  ASSERT_MAIN_THREAD()
  return callbackRegistryTable.exists(id);
}

// [[Rcpp::export(rng = false)]]
Rcpp::List list_queue_(int id) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(id);
  if (registry == nullptr) {
    Rcpp::stop("CallbackRegistry does not exist.");
  }
  return registry->list();
}


// Execute callbacks for a single event loop.
bool execCallbacksOne(
  bool runAll,
  shared_ptr<CallbackRegistry> callback_registry,
  Timestamp now
) {
  ASSERT_MAIN_THREAD()
  // execCallbacks can be called directly from C code, and the callbacks may
  // include Rcpp code. (Should we also call wrap?)
  Rcpp::RNGScope rngscope;
  ProtectCallbacks pcscope;

  // Set current loop for the duration of this function.
  CurrentRegistryGuard current_registry_guard(callback_registry->getId());

  do {
    // We only take one at a time, because we don't want to lose callbacks if
    // one of the callbacks throws an error
    Callback_sp callback = callback_registry->pop(now);
    if (callback == nullptr) {
      break;
    }

    // This line may throw errors!
    callback->invoke();

  } while (runAll);

  // I think there's no need to lock this since it's only modified from the
  // main thread. But need to check.
  std::vector<std::shared_ptr<CallbackRegistry> > children = callback_registry->children;
  for (std::vector<std::shared_ptr<CallbackRegistry> >::iterator it = children.begin();
       it != children.end();
       ++it)
  {
    execCallbacksOne(true, *it, now);
  }

  return true;
}

// Execute callbacks for an event loop and its children.
// [[Rcpp::export(rng = false)]]
bool execCallbacks(double timeoutSecs, bool runAll, int loop_id) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(loop_id);
  if (registry == nullptr) {
    Rcpp::stop("CallbackRegistry does not exist.");
  }

  if (!registry->wait(timeoutSecs, true)) {
    return false;
  }

  Timestamp now;
  execCallbacksOne(runAll, registry, now);

  // Call this now, in case any CallbackRegistries which have no R references
  // have emptied.
  callbackRegistryTable.pruneRegistries();
  return true;
}


// This function is called from the input handler on Unix, or the Windows
// equivalent. It may throw exceptions.
//
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
    if (!execCallbacks(0, true, GLOBAL_LOOP))
      return any;
    any = true;
  }
  return any;
}

// [[Rcpp::export(rng = false)]]
bool idle(int loop_id) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(loop_id);
  if (registry == nullptr) {
    Rcpp::stop("CallbackRegistry does not exist.");
  }
  return registry->empty();
}


static bool initialized = false;
// [[Rcpp::export(rng = false)]]
void ensureInitialized() {
  if (initialized) {
    return;
  }
  REGISTER_MAIN_THREAD()

  // Note that the global registry is not created here, but in R, from the
  // .onLoad function.
  setCurrentRegistryId(GLOBAL_LOOP);

  // Call the platform-specific initialization for the mechanism that runs the
  // event loop when the console is idle.
  ensureAutorunnerInitialized();
  initialized = true;
}

// [[Rcpp::export(rng = false)]]
std::string execLater(Rcpp::Function callback, double delaySecs, int loop_id) {
  ASSERT_MAIN_THREAD()
  ensureInitialized();
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(loop_id);
  if (registry == nullptr) {
    Rcpp::stop("CallbackRegistry does not exist.");
  }
  uint64_t callback_id = doExecLater(registry, callback, delaySecs, true);

  // We have to convert it to a string in order to maintain 64-bit precision,
  // since R doesn't support 64 bit integers.
  return toString(callback_id);
}



bool cancel(uint64_t callback_id, int loop_id) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(loop_id);
  if (registry == nullptr) {
    return false;
  }
  return registry->cancel(callback_id);
}

// [[Rcpp::export(rng = false)]]
bool cancel(std::string callback_id_s, int loop_id) {
  ASSERT_MAIN_THREAD()
  uint64_t callback_id;
  std::istringstream iss(callback_id_s);
  iss >> callback_id;

  // If the input is good (just a number with no other text) then eof will be
  // 1 and fail will be 0.
  if (! (iss.eof() && !iss.fail())) {
    return false;
  }

  return cancel(callback_id, loop_id);
}



// [[Rcpp::export(rng = false)]]
double nextOpSecs(int loop_id) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.getRegistry(loop_id);
  if (registry == nullptr) {
    Rcpp::stop("CallbackRegistry does not exist.");
  }

  Optional<Timestamp> nextTime = registry->nextTimestamp();
  if (!nextTime.has_value()) {
    return R_PosInf;
  } else {
    Timestamp now;
    return nextTime->diff_secs(now);
  }
}

// Schedules a C function to execute on a specific event loop. Returns
// callback ID on success, or 0 on error.
extern "C" uint64_t execLaterNative2(void (*func)(void*), void* data, double delaySecs, int loop_id) {
  ensureInitialized();
  return callbackRegistryTable.scheduleCallback(func, data, delaySecs, loop_id);
}

extern "C" int apiVersion() {
  return LATER_DLL_API_VERSION;
}
