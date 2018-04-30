#ifndef _WIN32

#include <errno.h>
#include <sys/time.h>

#include "timer_posix.h"
#include "timeconv.h"

void* Timer::bg_main_func(void* data) {
  reinterpret_cast<Timer*>(data)->bg_main();
  return 0;
}

void Timer::bg_main() {
  pthread_mutex_lock(&this->mutex);
  while (true) {
    
    // Guarded wait; we can't pass here until either the timer is stopped or we
    // have a wait time.
    while (!(this->stopped || this->wakeAt != boost::none)) {
      pthread_cond_wait(&this->cond, &this->mutex);
    }
    
    // We're stopped; return, which ends the thread.
    if (this->stopped) {
      return;
    }
    
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
      gettimeofday(&tv, NULL);
      timespec ts = timevalToTimespec(tv);
      ts.tv_sec += (time_t)secs;
      ts.tv_nsec += (secs - (time_t)secs) * 1e9;
      if (ts.tv_nsec < 0) {
        ts.tv_nsec += 1e9;
        ts.tv_sec--;
      }
      if (ts.tv_nsec >= 1e9) {
        ts.tv_nsec -= 1e9;
        ts.tv_sec++;
      }

      int res = pthread_cond_timedwait(&this->cond, &this->mutex, &ts);
      if (this->stopped) {
        return;
      }
      if (ETIMEDOUT != res) {
        // Time didn't elapse, we were woken up (probably). Start over.
        continue;
      }
    }

    this->wakeAt = boost::none;
    callback();
  }
}

Timer::Timer(const boost::function<void ()>& callback) :
  callback(callback), stopped(false) {
  
  pthread_mutex_init(&this->mutex, NULL);
  pthread_cond_init(&this->cond, NULL);
}

Timer::~Timer() {

  // Must stop background thread before cleaning up condition variable and
  // mutex. Calling pthread_cond_destroy on a condvar that's being waited
  // on results in undefined behavior--on Fedora 25+ it hangs.
  if (this->bgthread != boost::none) {
    pthread_mutex_lock(&this->mutex);
    this->stopped = true;
    pthread_cond_signal(&this->cond);
    pthread_mutex_unlock(&this->mutex);
  
    pthread_join(*this->bgthread, NULL);
  }

  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->mutex);
}

void Timer::set(const Timestamp& timestamp) {
  pthread_mutex_lock(&this->mutex);
  
  // If the thread has not yet been created, created it.
  if (this->bgthread == boost::none) {
    pthread_t thread;
    pthread_create(&thread, NULL, &bg_main_func, this);
    this->bgthread = thread;
  }
  
  this->wakeAt = timestamp;
  pthread_cond_signal(&this->cond);
  
  pthread_mutex_unlock(&this->mutex);
}

#endif // _WIN32
