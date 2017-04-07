#include <Rcpp.h>
#include <queue>
#include "timestamp.hpp"
#include "optional.hpp"

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

// Stores R function callbacks, ordered by timestamp.
class CallbackRegistry {
private:
  std::priority_queue<Callback,std::vector<Callback>,std::greater<Callback> > queue;
  
public:
  // Add a function to the registry, to be executed at `secs` seconds in
  // the future (i.e. relative to the current time).
  void add(Rcpp::Function func, double secs);
  
  // The smallest timestamp present in the registry, if any.
  // Use this to determine the next time we need to pump events.
  Optional<Timestamp> nextTimestamp() const;
  
  // Is the registry completely empty?
  bool empty() const;
  
  // Is anything ready to execute?
  bool due() const;
  
  // Pop and return an ordered list of functions to execute now.
  std::vector<Rcpp::Function> take();
};
