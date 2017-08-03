#ifndef _TIMER_POSIX_H_
#define _TIMER_POSIX_H_

#ifndef _WIN32

#include "timestamp.h"
#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <pthread.h>

class Timer {
  boost::function<void ()> callback;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  boost::optional<Timestamp> wakeAt;
  
  static void* bg_main_func(void*);
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
