#include <sys/time.h>
extern "C" {
#include "tinycthread.h"
}
#include <boost/noncopyable.hpp>

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
    if (clock_gettime(TIME_UTC, &ts) != 0) {
      throw std::runtime_error("clock_gettime failed");
    }
    ts.tv_sec += (time_t)timeoutSecs;
    ts.tv_nsec += (timeoutSecs - (time_t)timeoutSecs) * 1e9;
    if (ts.tv_nsec < 0) {
      ts.tv_nsec += 1e9;
      ts.tv_sec--;
    }
    if (ts.tv_nsec > 1e9) {
      ts.tv_nsec -= 1e9;
      ts.tv_sec++;
    }
    
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
