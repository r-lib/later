#include "callback_registry.h"

void CallbackRegistry::add(Rcpp::Function func, double secs) {
  Timestamp when(secs);
  Callback cb(when, new RcppFuncCallable(func));
  queue.push(cb);
}

void CallbackRegistry::add(void (*func)(void*), void* data, double secs) {
  Timestamp when(secs);
  Callback cb(when, new CFuncCallable(func, data));
  queue.push(cb);
}

// The smallest timestamp present in the registry, if any.
// Use this to determine the next time we need to pump events.
Optional<Timestamp> CallbackRegistry::nextTimestamp() const {
  if (this->queue.empty()) {
    return Optional<Timestamp>();
  } else {
    return Optional<Timestamp>(this->queue.top().when);
  }
}

bool CallbackRegistry::empty() const {
  return this->queue.empty();
}

// Returns true if the smallest timestamp exists and is not in the future.
bool CallbackRegistry::due() const {
  return !this->queue.empty() && !this->queue.top().when.future();
}

std::vector<Callback> CallbackRegistry::take() {
  std::vector<Callback> results;
  while (this->due()) {
    results.push_back(this->queue.top());
    this->queue.pop();
  }
  return results;
}
