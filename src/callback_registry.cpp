#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "callback_registry.h"
#include "debug.h"

CallbackRegistry::CallbackRegistry() : mutex(mtx_recursive), condvar(mutex) {
}

void CallbackRegistry::add(Rcpp::Function func, double secs) {
  // Copies of the Rcpp::Function should only be made on the main thread.
  ASSERT_MAIN_THREAD()
  Timestamp when(secs);
  Callback_sp cb = boost::make_shared<Callback>(when, func);
  Guard guard(mutex);
  queue.push(cb);
  condvar.signal();
}

void CallbackRegistry::add(void (*func)(void*), void* data, double secs) {
  Timestamp when(secs);
  Callback_sp cb = boost::make_shared<Callback>(when, boost::bind(func, data));
  Guard guard(mutex);
  queue.push(cb);
  condvar.signal();
}

// The smallest timestamp present in the registry, if any.
// Use this to determine the next time we need to pump events.
Optional<Timestamp> CallbackRegistry::nextTimestamp() const {
  Guard guard(mutex);
  if (this->queue.empty()) {
    return Optional<Timestamp>();
  } else {
    return Optional<Timestamp>(this->queue.top()->when);
  }
}

bool CallbackRegistry::empty() const {
  Guard guard(mutex);
  return this->queue.empty();
}

// Returns true if the smallest timestamp exists and is not in the future.
bool CallbackRegistry::due(const Timestamp& time) const {
  Guard guard(mutex);
  return !this->queue.empty() && !(this->queue.top()->when > time);
}

std::vector<Callback_sp> CallbackRegistry::take(size_t max, const Timestamp& time) {
  ASSERT_MAIN_THREAD()
  Guard guard(mutex);
  std::vector<Callback_sp> results;
  while (this->due(time) && (max <= 0 || results.size() < max)) {
    results.push_back(this->queue.top());
    this->queue.pop();
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
