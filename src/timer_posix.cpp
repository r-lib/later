#ifndef _WIN32

#include <errno.h>
#include <sys/time.h>

#include "timer_posix.h"

void* Timer::bg_main_func(void* data) {
  reinterpret_cast<Timer*>(data)->bg_main();
  return 0;
}

void Timer::bg_main() {
  pthread_mutex_lock(&this->mutex);
  while (true) {
    if (this->wakeAt == boost::none) {
      // The wake time has not been set yet. Wait until it is set, then start
      // the loop over again, so we hit the other branch.
      pthread_cond_wait(&this->cond, &this->mutex);
      continue;
    } else {
      // The wake time has been set. There are three possibilities:
      // 1. The wake time is in the past. Go ahead and execute now.
      // 2. Wait for the wake time, but we're notified before time elapses.
      //    Start the loop again.
      // 3. Wait for the wake time, and time elapses. Go ahead and execute now.
      double secs = (*this->wakeAt).diff_secs(Timestamp());
      if (secs > 0) {
        // Sadly, the pthread_cond_timedwait API requires an absolute time, not
        // a relative time. gettimeofday is the only way to do this that works
        // on both Linux and Darwin.
        timeval tv;
        timespec ts;
        gettimeofday(&tv, NULL);
        TIMEVAL_TO_TIMESPEC(&tv, &ts);
        ts.tv_sec += (time_t)secs;
        ts.tv_nsec += (secs - (time_t)secs) * 1e9;
        if (ts.tv_nsec < 0) {
          ts.tv_nsec += 1e9;
          ts.tv_sec--;
        }
        if (ts.tv_nsec > 1e9) {
          ts.tv_nsec -= 1e9;
          ts.tv_sec++;
        }

        int res = pthread_cond_timedwait(&this->cond, &this->mutex, &ts);
        if (ETIMEDOUT != res) {
          // Time didn't elapse, we were woken up (probably). Start over.
          continue;
        }
      }
    }

    this->wakeAt = boost::none;
    callback();
  }
}

Timer::Timer(const boost::function<void ()>& callback) :
  callback(callback) {
  
  pthread_mutex_init(&this->mutex, NULL);
  pthread_cond_init(&this->cond, NULL);
  
  pthread_t thread;
  pthread_create(&thread, NULL, &bg_main_func, this);
  pthread_detach(thread);
}

Timer::~Timer() {
  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->mutex);
}

void Timer::set(const Timestamp& timestamp) {
  pthread_mutex_lock(&this->mutex);
  
  this->wakeAt = timestamp;
  pthread_cond_signal(&this->cond);
  
  pthread_mutex_unlock(&this->mutex);
}

#endif // _WIN32
