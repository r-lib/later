#include <Rcpp.h>
#include <queue>
#include "timestamp.hpp"
#include "optional.hpp"

using namespace Rcpp;

struct Callback {
  Timestamp when;
  Rcpp::Function func;
  
  bool operator<(const Callback& other) const {
    return this->when < other.when;
  }
  
  bool operator>(const Callback& other) const {
    return this->when > other.when;
  }
};

class CallbackRegistry {
private:
  std::priority_queue<Callback,std::vector<Callback>,std::greater<Callback> > queue;
  
public:
  void add(Rcpp::Function func, double secs) {
    Timestamp when(secs);
    Callback cb = {when, func};
    queue.push(cb);
  }
  
  // The smallest timestamp present in the registry, if any.
  // Use this to determine the next time we need to pump events.
  Optional<Timestamp> nextTimestamp() const {
    if (this->queue.empty()) {
      return Optional<Timestamp>();
    } else {
      return Optional<Timestamp>(this->queue.top().when);
    }
  }
  
  bool empty() const {
    return this->queue.empty();
  }
  
  // Returns true if the smallest timestamp exists and is not in the future.
  bool due() const {
    return !this->queue.empty() && !this->queue.top().when.future();
  }
  
  std::vector<Rcpp::Function> take() {
    std::vector<Rcpp::Function> results;
    while (this->due()) {
      results.push_back(this->queue.top().func);
      this->queue.pop();
    }
    return results;
  }
};
