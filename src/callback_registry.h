#ifndef _CALLBACK_REGISTRY_H_
#define _CALLBACK_REGISTRY_H_

#include <Rcpp.h>
#include <queue>
#include <boost/operators.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include "timestamp.h"
#include "optional.h"
#include "threadutils.h"

// Callback is an abstract class with two subclasses. The reason that there
// are two subclasses is because one of them is for C++ (boost::function)
// callbacks, and the other is for R (Rcpp::Function) callbacks. Because
// Callbacks can be created from either the main thread or a background
// thread, the top-level Callback class cannot contain any Rcpp objects --
// otherwise R objects could be allocated on a background thread, which will
// cause memory corruption.

class Callback {

public:
  virtual ~Callback() {};
  Callback(Timestamp when) : when(when) {};

  bool operator<(const Callback& other) const {
    return this->when < other.when ||
      (!(this->when > other.when) && this->callbackId < other.callbackId);
  }

  bool operator>(const Callback& other) const {
    return other < *this;
  }

  uint64_t getCallbackId() const {
    return callbackId;
  };

  virtual void invoke() const = 0;

  void invoke_wrapped() const;

  virtual Rcpp::RObject rRepresentation() const = 0;

  Timestamp when;

protected:
  // Used to break ties when comparing to a callback that has precisely the same
  // timestamp
  uint64_t callbackId;
};


class BoostFunctionCallback : public Callback {
public:
  BoostFunctionCallback(Timestamp when, boost::function<void (void)> func);

  void invoke() const {
    func();
  }

  Rcpp::RObject rRepresentation() const;

private:
  boost::function<void (void)> func;
};


class RcppFunctionCallback : public Callback {
public:
  RcppFunctionCallback(Timestamp when, Rcpp::Function func);

  void invoke() const {
    func();
  }

  Rcpp::RObject rRepresentation() const;

private:
  Rcpp::Function func;
};



typedef boost::shared_ptr<Callback> Callback_sp;

template <typename T>
struct pointer_less_than {
  const bool operator()(const T a, const T b) const {
    return *a < *b;
  }
};


// Stores R function callbacks, ordered by timestamp.
class CallbackRegistry {
private:
  // Most of the behavior of the registry is like a priority queue. However, a
  // std::priority_queue only allows access to the top element, and when we
  // cancel a callback or get an Rcpp::List representation, we need random
  // access, so we'll use a std::set.
  typedef std::set<Callback_sp, pointer_less_than<Callback_sp> > cbSet;
  // This is a priority queue of shared pointers to Callback objects. The
  // reason it is not a priority_queue<Callback> is because that can cause
  // objects to be copied on the wrong thread, and even trigger an R GC event
  // on the wrong thread. https://github.com/r-lib/later/issues/39
  cbSet queue;
  mutable Mutex mutex;
  mutable ConditionVariable condvar;

public:
  CallbackRegistry();

  CallbackRegistry(boost::shared_ptr<CallbackRegistry> parent);

  // Add a function to the registry, to be executed at `secs` seconds in
  // the future (i.e. relative to the current time).
  uint64_t add(Rcpp::Function func, double secs);

  // Add a C function to the registry, to be executed at `secs` seconds in
  // the future (i.e. relative to the current time).
  uint64_t add(void (*func)(void*), void* data, double secs);

  bool cancel(uint64_t id);

  // The smallest timestamp present in the registry, if any.
  // Use this to determine the next time we need to pump events.
  Optional<Timestamp> nextTimestamp(bool recursive = true) const;

  // Is the registry completely empty?
  bool empty() const;

  // Is anything ready to execute?
  bool due(const Timestamp& time = Timestamp(), bool recursive = true) const;

  // Pop and return an ordered list of functions to execute now.
  std::vector<Callback_sp> take(size_t max = -1, const Timestamp& time = Timestamp());

  // Wait until the next available callback is ready to execute.
  bool wait(double timeoutSecs, bool recursive) const;

  // Return a List of items in the queue.
  Rcpp::List list() const;

  // References to parent and children registries. These are used for
  // automatically running child loops. The should only be accessed and
  // modified from the main thread. (Is that true?)
  boost::weak_ptr<CallbackRegistry> parent;
  std::vector<boost::weak_ptr<CallbackRegistry>> children;

  // To let another object signal this one. It will also signal its parent.
  void signal(bool recursive = true);
};

#endif // _CALLBACK_REGISTRY_H_
