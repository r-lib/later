#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <vector>
#include "callback_registry.h"
#include "debug.h"

#if __cplusplus >= 201103L
  #include <atomic>
  std::atomic<uint64_t> nextCallbackId(1);
#else
  // Fall back to boost::atomic if std::atomic isn't available. We want to
  // avoid boost::atomic when possible because on ARM, it requires the
  // -lboost_atomic linker flag. (https://github.com/r-lib/later/issues/73)
  #include <boost/atomic.hpp>
  boost::atomic<uint64_t> nextCallbackId(1);
#endif

// ============================================================================
// BoostFunctionCallback
// ============================================================================

BoostFunctionCallback::BoostFunctionCallback(Timestamp when, boost::function<void(void)> func) :
  Callback(when),
  func(func)
{
  this->callbackId = nextCallbackId++;
}

Rcpp::RObject BoostFunctionCallback::rRepresentation() const {
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
  std::vector<BoostFunctionCallback> callbacks;
  Timestamp ts;
  boost::function<void(void)> func;
  for (size_t i = 0; i < 100; i++) {
    callbacks.push_back(BoostFunctionCallback(ts, func));
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

CallbackRegistry::CallbackRegistry() : mutex(tct_mtx_recursive), condvar(mutex) {
}

uint64_t CallbackRegistry::add(Rcpp::Function func, double secs) {
  // Copies of the Rcpp::Function should only be made on the main thread.
  ASSERT_MAIN_THREAD()
  Timestamp when(secs);
  Callback_sp cb = boost::make_shared<RcppFunctionCallback>(when, func);
  Guard guard(mutex);
  queue.insert(cb);
  condvar.signal();
  return cb->getCallbackId();
}

uint64_t CallbackRegistry::add(void (*func)(void*), void* data, double secs) {
  Timestamp when(secs);
  Callback_sp cb = boost::make_shared<BoostFunctionCallback>(when, boost::bind(func, data));
  Guard guard(mutex);
  queue.insert(cb);
  condvar.signal();
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
Optional<Timestamp> CallbackRegistry::nextTimestamp() const {
  Guard guard(mutex);
  if (this->queue.empty()) {
    return Optional<Timestamp>();
  } else {
    cbSet::const_iterator it = queue.begin();
    return Optional<Timestamp>((*it)->when);
  }
}

bool CallbackRegistry::empty() const {
  Guard guard(mutex);
  return this->queue.empty();
}

// Returns true if the smallest timestamp exists and is not in the future.
bool CallbackRegistry::due(const Timestamp& time) const {
  Guard guard(mutex);
  cbSet::const_iterator it = queue.begin();
  return !this->queue.empty() && !((*it)->when > time);
}

std::vector<Callback_sp> CallbackRegistry::take(size_t max, const Timestamp& time) {
  ASSERT_MAIN_THREAD()
  Guard guard(mutex);
  std::vector<Callback_sp> results;
  while (this->due(time) && (max <= 0 || results.size() < max)) {
    cbSet::iterator it = queue.begin();
    results.push_back(*it);
    this->queue.erase(it);
  }
  return results;
}

bool CallbackRegistry::wait(double timeoutSecs) const {
  ASSERT_MAIN_THREAD()
  if (timeoutSecs < 0) {
    // "1000 years ought to be enough for anybody" --Bill Gates
    timeoutSecs = 3e10;
  }

  Timestamp expireTime(timeoutSecs);

  Guard guard(mutex);
  while (true) {
    Timestamp end = expireTime;
    Optional<Timestamp> next = nextTimestamp();
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
    condvar.timedwait(waitFor);
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
