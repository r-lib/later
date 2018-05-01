#ifndef _THREADUTILS_H_
#define _THREADUTILS_H_

#include <stdexcept>
#include <sys/time.h>
#include <boost/noncopyable.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_signed.hpp>

#include "tinycthread.h"
#include "timeconv.h"

#ifndef CLOCK_REALTIME
// This is only here to prevent compilation errors on Windows and older
// versions of OS X. clock_gettime doesn't exist on those platforms so
// tinycthread emulates it; the emulated versions don't pay attention
// to the clkid argument, but we still have to pass something.
#define CLOCK_REALTIME 0
#endif

class ConditionVariable;

class Mutex : boost::noncopyable {
  friend class ConditionVariable;
  mtx_t _m;
  
public:
  // type must be one of:
  //
  //   * mtx_plain for a simple non-recursive mutex
  //   * mtx_timed for a non-recursive mutex that supports timeout
  //   * mtx_try for a non-recursive mutex that supports test and return
  //   * mtx_plain | mtx_recursive (same as mtx_plain, but recursive)
  //   * mtx_timed | mtx_recursive (same as mtx_timed, but recursive)
  //   * mtx_try | mtx_recursive (same as mtx_try, but recursive)
  //
  // (although mtx_timed seems not to be actually implemented)
  Mutex(int type) {
    if (mtx_init(&_m, type) != thrd_success) {
      throw std::runtime_error("Mutex creation failed");
    }
  }
  
  virtual ~Mutex() {
    mtx_destroy(&_m);
  }
  
  void lock() {
    if (mtx_lock(&_m) != thrd_success) {
      throw std::runtime_error("Mutex failed to lock");
    }
  }
  
  bool tryLock() {
    int res = mtx_trylock(&_m);
    if (res == thrd_success) {
      return true;
    } else if (res == thrd_busy) {
      return false;
    } else {
      throw std::runtime_error("Mutex failed to trylock");
    }
  }
  
  void unlock() {
    if (mtx_unlock(&_m) != thrd_success) {
      throw std::runtime_error("Mutex failed to unlock");
    }
  }
};

class Guard : boost::noncopyable {
  Mutex* _mutex;
  
public:
  Guard(Mutex& mutex) : _mutex(&mutex) {
    _mutex->lock();
  }
  
  ~Guard() {
    _mutex->unlock();
  }
};

class ConditionVariable : boost::noncopyable {
  mtx_t* _m;
  cnd_t _c;
  
public:
  ConditionVariable(Mutex& mutex) : _m(&mutex._m) {
    // If time_t isn't integral, our addSeconds logic needs to change,
    // as it relies on casting to time_t being a truncation.
    if (!boost::is_integral<time_t>::value)
      throw std::runtime_error("Integral time_t type expected");
    // If time_t isn't signed, our addSeconds logic can't handle
    // negative values for secs.
    if (!boost::is_signed<time_t>::value)
      throw std::runtime_error("Signed time_t type expected");
    
    if (cnd_init(&_c) != thrd_success)
      throw std::runtime_error("Condition variable failed to initialize");
  }
  
  virtual ~ConditionVariable() {
    cnd_destroy(&_c);
  }
  
  // Unblocks one thread (if any are waiting)
  void signal() {
    if (cnd_signal(&_c) != thrd_success)
      throw std::runtime_error("Condition variable failed to signal");
  }
  
  // Unblocks all waiting threads
  void broadcast() {
    if (cnd_broadcast(&_c) != thrd_success)
      throw std::runtime_error("Condition variable failed to broadcast");
  }
  
  void wait() {
    if (cnd_wait(&_c, _m) != thrd_success)
      throw std::runtime_error("Condition variable failed to wait");
  }
  
  bool timedwait(double timeoutSecs) {
    timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
      throw std::runtime_error("clock_gettime failed");
    }
    
    ts = addSeconds(ts, timeoutSecs);

    int res = cnd_timedwait(&_c, _m, &ts);
    if (res == thrd_success) {
      return true;
    } else if (res == thrd_timeout) {
      return false;
    } else {
      throw std::runtime_error("Condition variable failed to timedwait");
    }
  }
};

#endif // _THREADUTILS_H_
