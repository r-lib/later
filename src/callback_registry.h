#include <Rcpp.h>
#include <queue>
#include "timestamp.h"
#include "optional.h"
#include "tinythread.h"

class Callable {
public:
  ~Callable() {}
  virtual void operator()() = 0;
};

class RcppFuncCallable : public Callable {
public:
  RcppFuncCallable(Rcpp::Function func) : func(func) {
  }
  
  virtual void operator()() {
    func();
  }
private:
  Rcpp::Function func;
};

class CFuncCallable : public Callable {
public:
  CFuncCallable(void (*func)(void*), void* data) :
    func(func), data(data) {
  }
  
  virtual void operator()() {
    func(data);
  }
private:
  void (*func)(void*);
  void* data;
};

class Callback {

public:
  Callback(Timestamp when, Callable* func) : when(when), func(func) {}
  
  bool operator<(const Callback& other) const {
    return this->when < other.when;
  }
  
  bool operator>(const Callback& other) const {
    return this->when > other.when;
  }
  
  void operator()() const {
    (*func)();
  }

  Timestamp when;
private:
  std::shared_ptr<Callable> func;
  
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
