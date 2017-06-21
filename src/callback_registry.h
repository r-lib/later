#include <Rcpp.h>
#include <queue>
#include <boost/function.hpp>
#include "timestamp.h"
#include "optional.h"
#include "tinythread.h"

typedef boost::function0<void> Task;

namespace {
  void execCallback(void* data) {
    Task* pTask = static_cast<Task*>(data);
    (*pTask)();
  }
} // namespace

class Callback {

public:
  Callback(Timestamp when, Task func) : when(when), func(func) {}
  
  bool operator<(const Callback& other) const {
    return this->when < other.when;
  }
  
  bool operator>(const Callback& other) const {
    return this->when > other.when;
  }
  
  void operator()() const {
    // Using R_ToplevelExec in an (unsuccessful) attempt to catch and properly
    // handle errors
    R_ToplevelExec(execCallback, (void*)&func);
  }

  Timestamp when;
  
private:
  Task func;
};

// Stores R function callbacks, ordered by timestamp.
class CallbackRegistry {
private:
  std::priority_queue<Callback,std::vector<Callback>,std::greater<Callback> > queue;
  mutable tthread::recursive_mutex mutex;
  
public:
  // Add a function to the registry, to be executed at `secs` seconds in
  // the future (i.e. relative to the current time).
  void add(Rcpp::Function func, double secs);
  
  // Add a C function to the registry, to be executed at `secs` seconds in
  // the future (i.e. relative to the current time).
  void add(void (*func)(void*), void* data, double secs);
  
  // The smallest timestamp present in the registry, if any.
  // Use this to determine the next time we need to pump events.
  Optional<Timestamp> nextTimestamp() const;
  
  // Is the registry completely empty?
  bool empty() const;
  
  // Is anything ready to execute?
  bool due() const;
  
  // Pop and return an ordered list of functions to execute now.
  std::vector<Callback> take();
};
