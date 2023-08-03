#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "callback_registry.h"
#include "debug.h"

static std::atomic<uint64_t> nextCallbackId(1);

// ============================================================================
// Invoke functions
// ============================================================================

enum InvokeResult {
  INVOKE_IN_PROGRESS,
  INVOKE_INTERRUPTED,
  INVOKE_ERROR,
  INVOKE_CPP_ERROR,
  INVOKE_COMPLETED
};

// This is set by invoke_c(). I
static InvokeResult last_invoke_result;
static std::string last_invoke_message;

// A wrapper for calling R_CheckUserInterrupt via R_ToplevelExec.
void checkInterruptFn(void*) {
  R_CheckUserInterrupt();
}

// The purpose of this function is to provide a plain C function to be called
// by R_ToplevelExec. Because it's called as a C function, it must not throw
// exceptions. Because this function returns void, the way for it to report
// the result to its caller is by setting last_invoke_result.
//
// This code needs to be able to handle interrupts, R errors, and C++
// exceptions. There are many ways these things can happen.
//
// * If the Callback object is a RcppFunctionCallback, then in the case of an
//   interrupt or an R error, it will throw a C++ exception. These exceptions
//   are the ones defined by Rcpp, and they will be caught by the try-catch in
//   this function.
// * It could be a StdFunctionCallback with C or C++ code.
//   * If the function invokes an Rcpp::Function and an interrupt or R error
//     happens within the Rcpp::Function, it will throw exceptions just like
//     the RcppFunctionCallback case, and they will be caught.
//   * If some other C++ exception occurs, it will be caught.
//   * If an interrupt (Ctrl-C, or Esc in RStudio) is received (outside of an
//     Rcpp::Function), this function will continue through to the end (and
//     set the state to INVOKE_COMPLETED). Later, when the invoke_wrapper()
//     function (which called this one) checks to see if the interrupt
//     happened, it will set the state to INVOKE_INTERRUPTED. (Note that it is
//     potentially possible for an interrupt and an exception to occur, in
//     which case we set the state to INVOKE_ERROR.)
//   * If the function calls R code with Rf_eval(), an interrupt or R error
//     could occur. If it's an interrupt, then it will be detect as in the
//     previous case. If an error occurs, then that error will be detected by
//     the invoke_wrapper() function (which called this one) and the state
//     will be set to INVOKE_ERROR.
//
// Note that the last case has one potentially problematic issue. If an error
// occurs in R code, then it will longjmp out of of this function, back to its
// caller, invoke_wrapped(). This will longjmp out of a try statement, which
// is generally not a good idea. We don't know ahead of time whether the
// Callback may longjmp or throw an exception -- some Callbacks could
// potentially do both.
//
// The alternative is to move the try-catch out of this function and into
// invoke_wrapped(), surrounding the `R_ToplevelExec(invoke_c, ...)`. However,
// if we do this, then exceptions would pass through the R_ToplevelExec, which
// is dangerous because it is plain C code. The current way of doing it is
// imperfect, but less dangerous.
//
// There does not seem to be a 100% safe way to call functions which could
// either longjmp or throw exceptions. If we do figure out a way to do that,
// it should be used here.
extern "C" void invoke_c(void* callback_p) {
  ASSERT_MAIN_THREAD()
  last_invoke_result = INVOKE_IN_PROGRESS;
  last_invoke_message = "";

  Callback* cb_p = (Callback*)callback_p;

  try {
    cb_p->invoke();
  }
  catch(Rcpp::internal::InterruptedException &e) {
    // Reaches here if the callback is in Rcpp code and an interrupt occurs.
    DEBUG_LOG("invoke_c: caught Rcpp::internal::InterruptedException", LOG_INFO);
    last_invoke_result = INVOKE_INTERRUPTED;
    return;
  }
  catch(Rcpp::eval_error &e) {
    // Reaches here if an R-level error happens in an Rcpp::Function.
    DEBUG_LOG("invoke_c: caught Rcpp::eval_error", LOG_INFO);
    last_invoke_result = INVOKE_ERROR;
    last_invoke_message = e.what();
    return;
  }
  catch(Rcpp::exception& e) {
    // Reaches here if an R-level error happens in an Rcpp::Function.
    DEBUG_LOG("invoke_c: caught Rcpp::exception", LOG_INFO);
    last_invoke_result = INVOKE_ERROR;
    last_invoke_message = e.what();
    return;
  }
  catch(std::exception& e) {
    // Reaches here if some other (non-Rcpp) C++ exception is thrown.
    DEBUG_LOG(std::string("invoke_c: caught std::exception: ") + typeid(e).name(),
              LOG_INFO);
    last_invoke_result = INVOKE_CPP_ERROR;
    last_invoke_message = e.what();
    return;
  }
  catch( ... ) {
    // Reaches here if a non-exception C++ object is thrown.
    DEBUG_LOG(std::string("invoke_c: caught unknown object: ") + typeid(std::current_exception()).name(),
              LOG_INFO);
    last_invoke_result = INVOKE_CPP_ERROR;
    return;
  }

  // Reaches here if no exceptions are thrown. It's possible to get here if an
  // interrupt was received outside of Rcpp code, or if an R error happened
  // using Rf_eval().
  DEBUG_LOG("invoke_c: COMPLETED", LOG_DEBUG);
  last_invoke_result = INVOKE_COMPLETED;
}

// Wrapper method for invoking a callback. The Callback object has an invoke()
// method, but instead of invoking it directly, this method should be used
// instead. The purpose of this method is to call invoke(), but wrap it in a
// R_ToplevelExec, so that any LONGJMPs (due to errors in R functions) won't
// cross that barrier in the call stack. If interrupts, exceptions, or
// LONGJMPs do occur, this function throws a C++ exception.
void Callback::invoke_wrapped() const {
  ASSERT_MAIN_THREAD()
  Rboolean result = R_ToplevelExec(invoke_c, (void*)this);

  if (!result) {
    DEBUG_LOG("invoke_wrapped: R_ToplevelExec return is FALSE; error or interrupt occurred in R code", LOG_INFO);
    last_invoke_result = INVOKE_ERROR;
  }

  if (R_ToplevelExec(checkInterruptFn, NULL) == FALSE) {
    // Reaches here if the callback is C/C++ code and an interrupt occurs.
    DEBUG_LOG("invoke_wrapped: interrupt (outside of R code) detected by R_CheckUserInterrupt", LOG_INFO);
    last_invoke_result = INVOKE_INTERRUPTED;
  }

  switch (last_invoke_result) {
  case INVOKE_INTERRUPTED:
    DEBUG_LOG("invoke_wrapped: throwing Rcpp::internal::InterruptedException", LOG_INFO);
    throw Rcpp::internal::InterruptedException();
  case INVOKE_ERROR:
    DEBUG_LOG("invoke_wrapped: throwing Rcpp::exception", LOG_INFO);
    throw Rcpp::exception(last_invoke_message.c_str());
  case INVOKE_CPP_ERROR:
    throw std::runtime_error("invoke_wrapped: throwing std::runtime_error");
  default:
    return;
  }
}


// ============================================================================
// StdFunctionCallback
// ============================================================================

StdFunctionCallback::StdFunctionCallback(Timestamp when, std::function<void(void)> func) :
  Callback(when),
  func(func)
{
  this->callbackId = nextCallbackId++;
}

Rcpp::RObject StdFunctionCallback::rRepresentation() const {
  using namespace Rcpp;
  ASSERT_MAIN_THREAD()

  return List::create(
    _["id"]       = callbackId,
    _["when"]     = when.diff_secs(Timestamp()),
    _["callback"] = Rcpp::CharacterVector::create("C/C++ function")
  );
}


// ============================================================================
// RcppFunctionCallback
// ============================================================================

RcppFunctionCallback::RcppFunctionCallback(Timestamp when, Rcpp::Function func) :
  Callback(when),
  func(func)
{
  ASSERT_MAIN_THREAD()
  this->callbackId = nextCallbackId++;
}

Rcpp::RObject RcppFunctionCallback::rRepresentation() const {
  using namespace Rcpp;
  ASSERT_MAIN_THREAD()

  return List::create(
    _["id"]       = callbackId,
    _["when"]     = when.diff_secs(Timestamp()),
    _["callback"] = func
  );
}


// ============================================================================
// CallbackRegistry
// ============================================================================

// [[Rcpp::export]]
void testCallbackOrdering() {
  std::vector<StdFunctionCallback> callbacks;
  Timestamp ts;
  std::function<void(void)> func;
  for (size_t i = 0; i < 100; i++) {
    callbacks.push_back(StdFunctionCallback(ts, func));
  }
  for (size_t i = 1; i < 100; i++) {
    if (callbacks[i] < callbacks[i-1]) {
      ::Rf_error("Callback ordering is broken [1]");
    }
    if (!(callbacks[i] > callbacks[i-1])) {
      ::Rf_error("Callback ordering is broken [2]");
    }
    if (callbacks[i-1] > callbacks[i]) {
      ::Rf_error("Callback ordering is broken [3]");
    }
    if (!(callbacks[i-1] < callbacks[i])) {
      ::Rf_error("Callback ordering is broken [4]");
    }
  }
  for (size_t i = 100; i > 1; i--) {
    if (callbacks[i-1] < callbacks[i-2]) {
      ::Rf_error("Callback ordering is broken [2]");
    }
  }
}

CallbackRegistry::CallbackRegistry(int id, Mutex* mutex, ConditionVariable* condvar)
  : id(id), mutex(mutex), condvar(condvar)
{
  ASSERT_MAIN_THREAD()
}

CallbackRegistry::~CallbackRegistry() {
  ASSERT_MAIN_THREAD()
}

int CallbackRegistry::getId() const {
  return id;
}

uint64_t CallbackRegistry::add(Rcpp::Function func, double secs) {
  // Copies of the Rcpp::Function should only be made on the main thread.
  ASSERT_MAIN_THREAD()
  Timestamp when(secs);
  Callback_sp cb = std::make_shared<RcppFunctionCallback>(when, func);
  Guard guard(mutex);
  queue.insert(cb);
  condvar->signal();

  return cb->getCallbackId();
}

uint64_t CallbackRegistry::add(void (*func)(void*), void* data, double secs) {
  Timestamp when(secs);
  Callback_sp cb = std::make_shared<StdFunctionCallback>(when, std::bind(func, data));
  Guard guard(mutex);
  queue.insert(cb);
  condvar->signal();

  return cb->getCallbackId();
}

bool CallbackRegistry::cancel(uint64_t id) {
  Guard guard(mutex);

  cbSet::const_iterator it;
  for (it = queue.begin(); it != queue.end(); ++it) {
    if ((*it)->getCallbackId() == id) {
      queue.erase(it);
      return true;
    }
  }

  return false;
}

// The smallest timestamp present in the registry, if any.
// Use this to determine the next time we need to pump events.
Optional<Timestamp> CallbackRegistry::nextTimestamp(bool recursive) const {
  Guard guard(mutex);

  Optional<Timestamp> minTimestamp;

  if (! this->queue.empty()) {
    cbSet::const_iterator it = queue.begin();
    minTimestamp = Optional<Timestamp>((*it)->when);
  }

  // Now check children
  if (recursive) {
    for (std::vector<std::shared_ptr<CallbackRegistry> >::const_iterator it = children.begin();
         it != children.end();
         ++it)
    {
      Optional<Timestamp> childNextTimestamp = (*it)->nextTimestamp(recursive);

      if (childNextTimestamp.has_value()) {
        if (minTimestamp.has_value()) {
          if (*childNextTimestamp < *minTimestamp) {
            minTimestamp = childNextTimestamp;
          }
        } else {
          minTimestamp = childNextTimestamp;
        }
      }
    }
  }

  return minTimestamp;
}

bool CallbackRegistry::empty() const {
  Guard guard(mutex);
  return this->queue.empty();
}

// Returns true if the smallest timestamp exists and is not in the future.
bool CallbackRegistry::due(const Timestamp& time, bool recursive) const {
  ASSERT_MAIN_THREAD()
  Guard guard(mutex);
  cbSet::const_iterator cbSet_it = queue.begin();
  if (!this->queue.empty() && !((*cbSet_it)->when > time)) {
    return true;
  }

  // Now check children
  if (recursive) {
    for (std::vector<std::shared_ptr<CallbackRegistry> >::const_iterator it = children.begin();
         it != children.end();
         ++it)
    {
      if ((*it)->due(time, true)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<Callback_sp> CallbackRegistry::take(size_t max, const Timestamp& time) {
  ASSERT_MAIN_THREAD()
  Guard guard(mutex);
  std::vector<Callback_sp> results;
  while (this->due(time, false) && (max <= 0 || results.size() < max)) {
    cbSet::iterator it = queue.begin();
    results.push_back(*it);
    this->queue.erase(it);
  }
  return results;
}

bool CallbackRegistry::wait(double timeoutSecs, bool recursive) const {
  ASSERT_MAIN_THREAD()
  if (timeoutSecs < 0) {
    // "1000 years ought to be enough for anybody" --Bill Gates
    timeoutSecs = 3e10;
  }

  Timestamp expireTime(timeoutSecs);

  Guard guard(mutex);
  while (true) {
    Timestamp end = expireTime;
    Optional<Timestamp> next = nextTimestamp(recursive);
    if (next.has_value() && *next < expireTime) {
      end = *next;
    }
    double waitFor = end.diff_secs(Timestamp());
    if (waitFor <= 0)
      break;
    // Don't wait for more than 2 seconds at a time, in order to keep us
    // at least somewhat responsive to user interrupts
    if (waitFor > 2) {
      waitFor = 2;
    }
    condvar->timedwait(waitFor);
    Rcpp::checkUserInterrupt();
  }

  return due();
}


Rcpp::List CallbackRegistry::list() const {
  ASSERT_MAIN_THREAD()
  Guard guard(mutex);

  Rcpp::List results;

  cbSet::const_iterator it;

  for (it = queue.begin(); it != queue.end(); it++) {
    results.push_back((*it)->rRepresentation());
  }

  return results;
}
