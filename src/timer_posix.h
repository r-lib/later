#ifndef _TIMER_POSIX_H_
#define _TIMER_POSIX_H_

#ifndef _WIN32

#include <boost/function.hpp>
#include <boost/optional.hpp>
#include "timestamp.h"
#include "threadutils.h"

class Timer {
  boost::function<void ()> callback;
  Mutex mutex;
  ConditionVariable cond;
  // Stores the handle to a bgthread, which is created upon demand. (Previously
  // the thread was created in the constructor, but addressed sanitized (ASAN)
  // builds of R would hang when pthread_create was called during dlopen.)
  boost::optional<thrd_t> bgthread;
  boost::optional<Timestamp> wakeAt;
  bool stopped;
  
  static int bg_main_func(void*);
  void bg_main();
public:
  Timer(const boost::function<void ()>& callback);
  virtual ~Timer();
  
  // Schedules the timer to fire next at the specified time.
  // If the timer is currently scheduled to fire, that will
  // be overwritten with this one (the timer only tracks one
  // timestamp at a time).
  void set(const Timestamp& timestamp);
};


#endif // _WIN32

#endif // _TIMER_POSIX_H_
