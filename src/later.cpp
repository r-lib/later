#include "later.h"
#include <Rcpp.h>
#include <map>
#include <queue>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/scope_exit.hpp>
#include "debug.h"
#include "utils.h"
#include "threadutils.h"

#include "callback_registry.h"
#include "interrupt.h"

using boost::shared_ptr;
using boost::weak_ptr;

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

shared_ptr<CallbackRegistry> xptrGetCallbackRegistry(SEXP registry_xptr);

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
// Global and current event loop
// ============================================================================
//
// In the R code, the term "loop" is used. In the C++ code, the terms "loop"
// and "registry" are both used. "Loop" is usually used when interfacing with
// R-facing event loop, and "registry" is usually used when interfacing with
// the implementation, which uses a callback registry.

shared_ptr<CallbackRegistry> global_registry;

void setGlobalRegistry(shared_ptr<CallbackRegistry> registry) {
  ASSERT_MAIN_THREAD()
  global_registry = registry;
}

shared_ptr<CallbackRegistry> getGlobalRegistry() {
  ASSERT_MAIN_THREAD()
  return global_registry;
}

bool existsGlobalRegistry() {
  ASSERT_MAIN_THREAD()
  return global_registry != nullptr;
}

// [[Rcpp::export]]
SEXP getGlobalRegistryXptr() {
  ASSERT_MAIN_THREAD()
  return global_registry->getXptr();
}


shared_ptr<CallbackRegistry> current_registry;

void setCurrentRegistry(shared_ptr<CallbackRegistry> registry) {
  ASSERT_MAIN_THREAD()
  current_registry = registry;
}

shared_ptr<CallbackRegistry> getCurrentRegistry() {
  ASSERT_MAIN_THREAD()
  return current_registry;
}

// [[Rcpp::export]]
void setCurrentRegistryXptr(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  setCurrentRegistry(xptrGetCallbackRegistry(registry_xptr));
}

// [[Rcpp::export]]
SEXP getCurrentRegistryXptr() {
  ASSERT_MAIN_THREAD()
  return current_registry->getXptr();
}

// Class for setting current registry and resetting when function exits, using
// RAII.
class CurrentRegistryGuard {
public:
  CurrentRegistryGuard(shared_ptr<CallbackRegistry> registry) {
    ASSERT_MAIN_THREAD()
    old_registry = getCurrentRegistry();
    setCurrentRegistry(registry);
  }
  ~CurrentRegistryGuard() {
    setCurrentRegistry(old_registry);
  }
private:
  shared_ptr<CallbackRegistry> old_registry;
};



// ============================================================================
// Callback registry table
// ============================================================================
//
// This class is used for accessing a registry by ID. In most cases, a
// registry is accessed from R by using an external pointer object, and from
// C++ by using a shared_ptr<CallbackRegistry>. However, there is one case
// where it needs to be accessed by ID.
//
// When C++ code in another package, or from a separate thread wants to call
// later() on a specific event loop, it must use an integer ID for the loop.
// Typically, an event loop would be created, the ID would be extracted and
// passed to C++ code, which would then call later() with the loop ID.
//
// The reason that the other package's C++ code must use an ID instead of xptr
// is because (A) an external pointer to the CallbackRegistry cannot be used
// from another thread (because it is an R object and it is not thread-safe to
// do so), and (B) it is not feasible to provide tools for the other package
// to extract the shared_ptr<CallbackRegistry> from the xptr. Doing the latter
// would require boost in inst/include/later.h.
//
// This class stores weak_ptrs to the CallbackRegistry objects, so that it
// will not prevent them from being deleted when they are no longer needed.
//
// The operations on this class are thread-safe, because they might be used to
// from another thread.
//
class CallbackRegistryTable {
public:
  CallbackRegistryTable() : mutex(tct_mtx_plain | tct_mtx_recursive) {
  }

  bool exists(int id) {
    Guard guard(mutex);
    return (registries.find(id) != registries.end());
  }

  bool add(int id, shared_ptr<CallbackRegistry> registry) {
    Guard guard(mutex);
    if (exists(id)) {
      Rcpp::stop("Can't create event loop %d because it already exists.", id);
    }
    registries[id] = weak_ptr<CallbackRegistry>(registry);
    return true;
  }

  // Returns a shared_ptr to the registry. If the registry is not present in
  // the table, or if the target CallbackRegistry has already been deleted,
  // then the shared_ptr is empty.
  shared_ptr<CallbackRegistry> get(int id) {
    Guard guard(mutex);
    if (!exists(id)) {
      return shared_ptr<CallbackRegistry>();
    }
    // If the target of the shared_ptr has been deleted, then this is an empty
    // shared_ptr.
    return registries[id].lock();
  }

  bool remove(int id) {
    Guard guard(mutex);
    int n = registries.erase(id);
    if (n == 0) return false;
    else return true;
  }

private:
  std::map<int, weak_ptr<CallbackRegistry> > registries;
  Mutex mutex;
};

static CallbackRegistryTable callbackRegistryTable;


// ============================================================================
// Callback registry
// ============================================================================

shared_ptr<CallbackRegistry> xptrGetCallbackRegistry(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  if (TYPEOF(registry_xptr) != EXTPTRSXP) {
    throw Rcpp::exception("Expected external pointer.");
  }
  shared_ptr<CallbackRegistry>* reg_p =
    reinterpret_cast<shared_ptr<CallbackRegistry>*>(R_ExternalPtrAddr(registry_xptr));

  // If the external pointer has already been cleared, return an empty shared_ptr.
  if (reg_p == nullptr) {
    return shared_ptr<CallbackRegistry>();
  }

  return *reg_p;
}

// This deletes a CallbackRegistry and deregisters it as a child of its
// parent. Any children of this registry are orphaned -- they no longer have a
// parent. (Maybe this should be an option?) The deletion consists of clearing
// the external pointer object (so the CallbackRegistry is no longer
// accessible from R), and deleting the xptr's pointer to the shared pointer
// to this object. When this function exits, there should be no more
// references to the CallbackRegistry and the destructor should run.
//
// [[Rcpp::export]]
bool deleteCallbackRegistry(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = xptrGetCallbackRegistry(registry_xptr);

  if (registry == nullptr) {
    return false;
  }

  if (registry == getGlobalRegistry()) {
    Rf_error("Can't delete global loop.");
  }
  if (registry == getCurrentRegistry()) {
    Rf_error("Can't delete current loop.");
  }

  // Deregister this object from its parent. Do it here instead of the in the
  // CallbackRegistry destructor, for two reasons: One is that we can be 100%
  // sure that the deregistration happens right now (it's possible that the
  // object's destructor won't run yet, because someone else has a shared_ptr
  // to it). Second, we can't reliably use a shared_ptr to the object from
  // inside its destructor; we need to some pointer comparison, but by the
  // time the destructor runs, you can't run shared_from_this() in the object,
  // because there are no more shared_ptrs to it.
  boost::shared_ptr<CallbackRegistry> parent = registry->parent;
  if (parent != nullptr) {
    // Remove this registry from the parent's list of children.
    for (std::vector<boost::shared_ptr<CallbackRegistry>>::iterator it = parent->children.begin();
         it != parent->children.end();
        )
    {
      if (*it == registry) {
        parent->children.erase(it);
        break;
      } else {
        ++it;
      }
    }
  }

  // Tell the children that they no longer have a parent.
  for (std::vector<boost::shared_ptr<CallbackRegistry>>::iterator it = registry->children.begin();
       it != registry->children.end();
       ++it)
  {
    (*it)->parent.reset();
  }

  callbackRegistryTable.remove(registry->getId());

  delete reinterpret_cast<shared_ptr<CallbackRegistry>*>(R_ExternalPtrAddr(registry_xptr));
  R_ClearExternalPtr(registry_xptr);

  return true;
}

// void wrapper for deleteCallbackRegistry(). This is the finalizer for the
// external pointer; it is called when the external pointer is GC'd. It is
// possible that deleteCallbackRegistry() has already been called before this
// one is called, but in that case, it simply returns and does nothing.
void registry_xptr_deleter(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  deleteCallbackRegistry(registry_xptr);
}


// [[Rcpp::export]]
SEXP createCallbackRegistry(int id, SEXP parent_loop_xptr) {
  ASSERT_MAIN_THREAD()
  CallbackRegistry* reg = new CallbackRegistry(id);
  shared_ptr<CallbackRegistry> *registry = new shared_ptr<CallbackRegistry>(reg);

  if (parent_loop_xptr != R_NilValue) {
    shared_ptr<CallbackRegistry> parent = xptrGetCallbackRegistry(parent_loop_xptr);
    (*registry)->parent = parent;
    parent->children.push_back(*registry);
  }

  callbackRegistryTable.add(id, *registry);

  SEXP registry_xptr = PROTECT(R_MakeExternalPtr(registry, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(registry_xptr, registry_xptr_deleter, FALSE);

  SEXP Rf_classgets(SEXP, SEXP);

  (*registry)->setXptr(registry_xptr);

  // Add S3 class
  Rf_classgets(registry_xptr, Rf_mkString("event_loop"));

  UNPROTECT(1);
  return registry_xptr;
}

void createGlobalRegistry() {
  ASSERT_MAIN_THREAD()
  if (existsGlobalRegistry()) {
    Rf_error("Can't create global loop because it already exists.");
  }
  SEXP registry_xptr = PROTECT(createCallbackRegistry(0, R_NilValue));

  // Make sure the external pointer never gets GC'd
  R_PreserveObject(registry_xptr);

  shared_ptr<CallbackRegistry> registry = xptrGetCallbackRegistry(registry_xptr);
  setGlobalRegistry(registry);
  setCurrentRegistry(registry);
  UNPROTECT(1);
}


// [[Rcpp::export]]
bool existsCallbackRegistry(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  bool exists = (xptrGetCallbackRegistry(registry_xptr) != nullptr);
  return exists;
}


// [[Rcpp::export]]
SEXP getLoopId(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  shared_ptr<CallbackRegistry> registry = xptrGetCallbackRegistry(registry_xptr);
  if (registry == nullptr) {
    return R_NilValue;
  }

  return Rf_ScalarInteger(registry->getId());
}


// [[Rcpp::export]]
Rcpp::List list_queue_(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  return xptrGetCallbackRegistry(registry_xptr)->list();
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
  CurrentRegistryGuard current_registry_guard(callback_registry);

  do {
    // We only take one at a time, because we don't want to lose callbacks if
    // one of the callbacks throws an error
    std::vector<Callback_sp> callbacks = callback_registry->take(1, now);
    if (callbacks.size() == 0) {
      break;
    }
    // This line may throw errors!
    callbacks[0]->invoke_wrapped();
  } while (runAll);

  // TODO: Recurse, but pass along `now`.
  // I think there's no need to lock this since it's only modified from the
  // main thread. But need to check.
  std::vector<boost::shared_ptr<CallbackRegistry> > children = callback_registry->children;
  for (std::vector<boost::shared_ptr<CallbackRegistry> >::iterator it = children.begin();
       it != children.end();
       ++it)
  {
    execCallbacksOne(true, *it, now);
  }

  return true;
}

bool execCallbacks(
  double timeoutSecs,
  bool runAll,
  shared_ptr<CallbackRegistry> callback_registry)
{
  ASSERT_MAIN_THREAD()

  if (!callback_registry->wait(timeoutSecs, true)) {
    return false;
  }

  Timestamp now;
  execCallbacksOne(runAll, callback_registry, now);

  // TODO: Fix return value
  return true;
}


// Execute callbacks for an event loop and its children.
// [[Rcpp::export]]
bool execCallbacks(double timeoutSecs, bool runAll, SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  return execCallbacks(timeoutSecs, runAll, xptrGetCallbackRegistry(registry_xptr));
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
    if (!execCallbacks(0, true, getGlobalRegistry()))
      return any;
    any = true;
  }
  return any;
}

// [[Rcpp::export]]
bool idle(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  return xptrGetCallbackRegistry(registry_xptr)->empty();
}


static bool initialized = false;
// [[Rcpp::export]]
void ensureInitialized() {
  if (initialized) {
    return;
  }
  REGISTER_MAIN_THREAD()
  if (!existsGlobalRegistry()) {
    createGlobalRegistry();
  }

  // Call the platform-specific initialization for the mechanism that runs the
  // event loop when the console is idle.
  ensureAutorunnerInitialized();

  initialized = true;
}

// [[Rcpp::export]]
std::string execLater(Rcpp::Function callback, double delaySecs, SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  ensureInitialized();
  shared_ptr<CallbackRegistry> registry = xptrGetCallbackRegistry(registry_xptr);
  if (registry == nullptr) {
    Rf_error("CallbackRegistry does not exist.");
  }
  uint64_t callback_id = doExecLater(registry, callback, delaySecs, true);

  // We have to convert it to a string in order to maintain 64-bit precision,
  // since R doesn't support 64 bit integers.
  return toString(callback_id);
}



bool cancel(uint64_t callback_id, SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()

  boost::shared_ptr<CallbackRegistry> reg = xptrGetCallbackRegistry(registry_xptr);
  if (!reg)
    return false;

  return reg->cancel(callback_id);
}

// [[Rcpp::export]]
bool cancel(std::string callback_id_s, SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  uint64_t callback_id;
  std::istringstream iss(callback_id_s);
  iss >> callback_id;

  // If the input is good (just a number with no other text) then eof will be
  // 1 and fail will be 0.
  if (! (iss.eof() && !iss.fail())) {
    return false;
  }

  return cancel(callback_id, registry_xptr);
}



// [[Rcpp::export]]
double nextOpSecs(SEXP registry_xptr) {
  ASSERT_MAIN_THREAD()
  Optional<Timestamp> nextTime = xptrGetCallbackRegistry(registry_xptr)->nextTimestamp();
  if (!nextTime.has_value()) {
    return R_PosInf;
  } else {
    Timestamp now;
    return nextTime->diff_secs(now);
  }
}

// Schedules a C function to execute on the global loop. Returns callback ID
// on success, or 0 on error.
extern "C" uint64_t execLaterNative(void (*func)(void*), void* data, double delaySecs) {
  return execLaterNative2(func, data, delaySecs, GLOBAL_LOOP_ID);
}

// Schedules a C function to execute on a specific event loop. Returns
// callback ID on success, or 0 on error.
extern "C" uint64_t execLaterNative2(void (*func)(void*), void* data, double delaySecs, int loop_id) {
  ensureInitialized();
  shared_ptr<CallbackRegistry> registry = callbackRegistryTable.get(loop_id);
  if (registry == nullptr) {
    return 0;
  }
  return doExecLater(registry, func, data, delaySecs, true);
}

extern "C" int apiVersion() {
  return LATER_DLL_API_VERSION;
}
