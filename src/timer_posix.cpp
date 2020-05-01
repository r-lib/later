#ifndef _WIN32

#include <errno.h>
#include <sys/time.h>

#include "timer_posix.h"

int Timer::bg_main_func(void* data) {
  reinterpret_cast<Timer*>(data)->bg_main();
  return 0;
}

void Timer::bg_main() {
  Guard guard(&this->mutex);
  while (true) {

    // Guarded wait; we can't pass here until either the timer is stopped or we
    // have a wait time.
    while (!(this->stopped || this->wakeAt != boost::none)) {
      this->cond.wait();
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
      bool signalled = this->cond.timedwait(secs);
      if (this->stopped) {
        return;
      }
      if (signalled) {
        // Time didn't elapse, we were woken up (probably). Start over.
        continue;
      }
    }

    this->wakeAt = boost::none;
    callback();
  }
}

Timer::Timer(const boost::function<void ()>& callback) :
  callback(callback), mutex(tct_mtx_recursive), cond(mutex), stopped(false) {
}

Timer::~Timer() {

  // Must stop background thread before cleaning up condition variable and
  // mutex. Calling pthread_cond_destroy on a condvar that's being waited
  // on results in undefined behavior--on Fedora 25+ it hangs.
  if (this->bgthread != boost::none) {
    {
      Guard guard(&this->mutex);
      this->stopped = true;
      this->cond.signal();
    }

    tct_thrd_join(*this->bgthread, NULL);
  }
}

void Timer::set(const Timestamp& timestamp) {
  Guard guard(&this->mutex);

  // If the thread has not yet been created, created it.
  if (this->bgthread == boost::none) {
    tct_thrd_t thread;
    tct_thrd_create(&thread, &bg_main_func, this);
    this->bgthread = thread;
  }

  this->wakeAt = timestamp;
  this->cond.signal();
}

#endif // _WIN32
