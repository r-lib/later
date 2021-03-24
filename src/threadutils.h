#ifndef _THREADUTILS_H_
#define _THREADUTILS_H_

#include <stdexcept>
#include <sys/time.h>
#include <type_traits>

#include "tinycthread.h"
#include "timeconv.h"

class ConditionVariable;

class Mutex {
  friend class ConditionVariable;
  tct_mtx_t _m;

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
    if (tct_mtx_init(&_m, type) != tct_thrd_success) {
      throw std::runtime_error("Mutex creation failed");
    }
  }

  // Make non-copyable
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  virtual ~Mutex() {
    tct_mtx_destroy(&_m);
  }

  void lock() {
    if (tct_mtx_lock(&_m) != tct_thrd_success) {
      throw std::runtime_error("Mutex failed to lock");
    }
  }

  bool tryLock() {
    int res = tct_mtx_trylock(&_m);
    if (res == tct_thrd_success) {
      return true;
    } else if (res == tct_thrd_busy) {
      return false;
    } else {
      throw std::runtime_error("Mutex failed to trylock");
    }
  }

  void unlock() {
    if (tct_mtx_unlock(&_m) != tct_thrd_success) {
      throw std::runtime_error("Mutex failed to unlock");
    }
  }
};

class Guard {
  Mutex* _mutex;

public:
  Guard(Mutex* mutex) : _mutex(mutex) {
    _mutex->lock();
  }

  // Make non-copyable
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;

  ~Guard() {
    _mutex->unlock();
  }
};

class ConditionVariable {
  tct_mtx_t* _m;
  tct_cnd_t _c;

public:
  ConditionVariable(Mutex& mutex) : _m(&mutex._m) {
    // If time_t isn't integral, our addSeconds logic needs to change,
    // as it relies on casting to time_t being a truncation.
    if (!std::is_integral<time_t>::value)
      throw std::runtime_error("Integral time_t type expected");
    // If time_t isn't signed, our addSeconds logic can't handle
    // negative values for secs.
    if (!std::is_signed<time_t>::value)
      throw std::runtime_error("Signed time_t type expected");

    if (tct_cnd_init(&_c) != tct_thrd_success)
      throw std::runtime_error("Condition variable failed to initialize");
  }

  // Make non-copyable
  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  virtual ~ConditionVariable() {
    tct_cnd_destroy(&_c);
  }

  // Unblocks one thread (if any are waiting)
  void signal() {
    if (tct_cnd_signal(&_c) != tct_thrd_success)
      throw std::runtime_error("Condition variable failed to signal");
  }

  // Unblocks all waiting threads
  void broadcast() {
    if (tct_cnd_broadcast(&_c) != tct_thrd_success)
      throw std::runtime_error("Condition variable failed to broadcast");
  }

  void wait() {
    if (tct_cnd_wait(&_c, _m) != tct_thrd_success)
      throw std::runtime_error("Condition variable failed to wait");
  }

  bool timedwait(double timeoutSecs) {
    timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
      throw std::runtime_error("timespec_get failed");
    }

    ts = addSeconds(ts, timeoutSecs);

    int res = tct_cnd_timedwait(&_c, _m, &ts);
    if (res == tct_thrd_success) {
      return true;
    } else if (res == tct_thrd_timedout) {
      return false;
    } else {
      throw std::runtime_error("Condition variable failed to timedwait");
    }
  }
};

#endif // _THREADUTILS_H_
