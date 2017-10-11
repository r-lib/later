#include <boost/bind.hpp>
#include "callback_registry.h"

typedef tthread::lock_guard<tthread::recursive_mutex> Guard;

void CallbackRegistry::add(Rcpp::Function func, double secs) {
  Timestamp when(secs);
  Callback cb(when, func);
  Guard guard(mutex);
  queue.push(cb);
}

void CallbackRegistry::add(void (*func)(void*), void* data, double secs) {
  Timestamp when(secs);
  Callback cb(when, boost::bind(func, data));
  Guard guard(mutex);
  queue.push(cb);
}

// The smallest timestamp present in the registry, if any.
// Use this to determine the next time we need to pump events.
Optional<Timestamp> CallbackRegistry::nextTimestamp() const {
  Guard guard(mutex);
  if (this->queue.empty()) {
    return Optional<Timestamp>();
  } else {
    return Optional<Timestamp>(this->queue.top().when);
  }
}

bool CallbackRegistry::empty() const {
  Guard guard(mutex);
  return this->queue.empty();
}

// Returns true if the smallest timestamp exists and is not in the future.
bool CallbackRegistry::due(const Timestamp& time) const {
  Guard guard(mutex);
  return !this->queue.empty() && !(this->queue.top().when > time);
}

std::vector<Callback> CallbackRegistry::take(size_t max, const Timestamp& time) {
  Guard guard(mutex);
  std::vector<Callback> results;
  while (this->due(time) && (max <= 0 || results.size() < max)) {
    results.push_back(this->queue.top());
    this->queue.pop();
  }
  return results;
}
